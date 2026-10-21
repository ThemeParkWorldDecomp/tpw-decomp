// Connection.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of Connection code, named after it.

#include "email.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_Online.h"
#include "LbDrawPrimitives.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// Unit v51015: base64enc 0x10202114, Email::Attachment::Attachment 0x10202670, Connection::Connection 0x10201a84,
// Connection::SendCommandLine 0x10201b14, CUIGrapher::DrawAxes 0x101fda4c.

namespace {

// The alphabet at 0x102b16c4 (TOC slot -0x3afc).
const char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// The texts at the TOC base 0x1024faeb: +0x14 "rb", +0x17 the file name, +0x24 the type.
const char kModeRead[] = "rb";
const char kFileName[] = "postcard.jpg";
const char kFileType[] = "image/jpeg";

}  // namespace

void base64enc(unsigned char* out, unsigned char* in, int count);

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

// Connection::RecvResponse and Connection::GetLocalIPAddress (main app, email.cpp).

// Reads the buffer line by line (CR LF ends a line), takes the first three digits as a reply code (100..599); a '-' in the 4th column
// marks a continued reply. Keeps the unread tail at the start of the buffer, receives more when no complete reply is there.
int32_t Connection::RecvResponse(char* out, int32_t size, bool flag) {
    int32_t code = -1;
    for (;;) {
        char* p = mBuf;
        char* line = p;
        int32_t i = 0;
        while (i < mLen && code == -1) {
            if (static_cast<uint8_t>(p[0]) == 0x0d && static_cast<uint8_t>(p[1]) == 0x0a) {
                p[0] = 0;
                p[1] = 0;
                i += 2;
                p += 2;
                code = ftp_callee::Atoi(line);
                if (code < 100 || code >= 0x258) code = -1;
                if (out != nullptr && ((code == -1 && !flag) || (code != -1 && flag))) ftp_callee::Strncpy(out, line, size);
                if (line[3] == 0x2d) code = -1;
                line = p;
            } else {
                p++;
                i++;
            }
        }
        if (line != mBuf) {
            mLen = mLen - static_cast<int32_t>(line - mBuf);
            if (mLen > 0) conn_callee::Memmove(mBuf, line, mLen);
        }
        if (code != -1) return code;
        int32_t got = ftp_callee::Recv(mSocket, mBuf + mLen, 0x3ff - mLen, 0);
        if (got <= 0) return 0;
        mLen = mLen + got;
        mBuf[mLen] = 0;
    }
}

// The address of this machine: from the host name, else the local end of a datagram socket connected to a fixed address.
uint32_t Connection::GetLocalIPAddress() {
    struct {
        uint16_t family;
        uint16_t port;
        uint32_t ip;
        uint8_t zero[8];
    } local = {};
    char name[128];
    int32_t length;
    if (conn_callee::Gethostname(name, 0x80) == 0) {
        FTPHostent* host = ftp_callee::GetHostByName(name);
        if (host != nullptr) local.ip = *host->mAddrList[0];
    }
    if (local.ip == 0) {
        length = 0x10;
        int32_t sock = ftp_callee::Socket(2, 2, 2);
        if (sock != -1) {
            struct {
                uint16_t family;
                uint16_t port;
                uint32_t ip;
                uint8_t zero[8];
            } peer = {};
            peer.family = 2;
            peer.port = 7;
            peer.ip = ftp_callee::InetAddr("128.127.50.1");   // 0x1024fa24
            if (ftp_callee::SockConnect(sock, &peer, 0x10) == 0) ftp_callee::GetSockName(sock, &local, &length);
            ftp_callee::CloseSocket(sock);
        }
    }
    return local.ip;
}

// CSock::CSock 0x10201f50 and CSock::SendData 0x10202058 (unit v5304).

CSock::CSock() {
    csock_callee::SockCtor(this);
    mVtable = 0x102b1638;   // TOC[-0x5f10]: the CSock vtable's address
    std::memset(mF08, 0, 0x10);
}

int32_t CSock::SendData(const char* data, int32_t count) {
    if (data == nullptr) return 1;
    while (count > 0) {
        int32_t chunk = count;
        if (chunk > 0x2000) chunk = 0x2000;
        int32_t sent = csock_callee::Send(mSocket, data, chunk, 0);
        if (sent < 0) return 1;
        data += sent;
        count -= sent;
    }
    return 0;
}
