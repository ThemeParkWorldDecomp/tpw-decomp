// FTPClient members, unit v5273: the constructor body, Disconnect / _Disconnect, Download, _CloseDSocket, ParseFileSize.
#include <cstdint>
#include <cstring>

#include "FS_Online.h"

namespace {
// The original reads the C library's character map (bits 2 and 4: white space). The host answers the same for the characters the map
// marks: tab to carriage return and the blank.
inline bool IsSpace(char c) {
    uint8_t u = static_cast<uint8_t>(c);
    return u == 32 || (u >= 9 && u <= 13);
}
}  // namespace

// 0x1020b368
FTPClient* FTPClient::Construct(CFTPControl* control, bool threaded) {
    ftp_callee::ConnectionCtor(this);
    mDataSocket = -1;
    mListenSocket = -1;
    mControl = control;
    ftp_callee::StringCtor(&mServer);
    ftp_callee::CondCtor(mCondA);
    ftp_callee::CondCtor(mCondB);
    mF276c = 0;
    mReady = 1;
    mThreadStarted = 0;
    mThreaded = threaded;
    std::memset(&mFamily, 0, 0x10);
    uint8_t wsaData[0x190];
    ftp_callee::WSAStartup(0x101, wsaData);
    return this;
}

// 0x1020beb8
int32_t FTPClient::Disconnect() {
    if (mThreaded == 0) return _Disconnect();
    if (mThreadStarted == 0) {
        if (ftp_callee::BthreadCreate(&FTPClient::_threadfn, this) != -1) mThreadStarted = 1;
    }
    while (mReady == 0) ftp_callee::Sleep(1);
    mReady = 0;
    mF276c = 2;
    ftp_callee::CondSignal(mCondA);
    return 0;
}

// 0x1020bf94
int32_t FTPClient::_Disconnect() {
    int32_t sock;
    std::memcpy(&sock, mF04, 4);   // the connection's socket, Mac offset 4
    if (sock == -1) return 0x38;
    ftp_callee::SendCommandLine(this, "QUIT\r\n");
    int32_t answer = ftp_callee::RecvResponse(this, nullptr, 0, true);
    if (answer != 0 && answer != 0xdd) return 0x43;
    ftp_callee::SockClose(this);
    _CloseDSocket(mListenSocket);
    return 0;
}

// 0x1020c064
int32_t FTPClient::Download(char* remote, char* local) {
    ftp_callee::Strncpy(mPath, local, 0x100);
    ftp_callee::Strncpy(mPath + 0x100, remote, 0x100);
    if (mThreaded == 0) return _Download();
    if (mThreadStarted == 0) {
        if (ftp_callee::BthreadCreate(&FTPClient::_threadfn, this) != -1) mThreadStarted = 1;
    }
    while (mReady == 0) ftp_callee::Sleep(1);
    mReady = 0;
    mF276c = 3;
    ftp_callee::CondSignal(mCondA);
    return 0;
}

// 0x1020c174: the receive buffer is the connection's at Mac offset 0x20 (0x400 bytes); the host uses mBuffer.
void FTPClient::_CloseDSocket(int32_t& sock) {
    if (sock == -1) return;
    ftp_callee::Shutdown(sock, 1);
    int32_t n;
    do {
        n = ftp_callee::Recv(sock, mBuffer, 0x400, 0);
    } while (n > 0);
    ftp_callee::CloseSocket(sock);
    sock = -1;
}

// 0x1020c744: skips four fields (and the blanks after each) of a listing line, then reads the number (no end test: runs on past
// the end of a short line, like the original).
int32_t FTPClient::ParseFileSize(char* line) {
    for (int field = 0; field < 4; ++field) {
        while (!IsSpace(*line)) ++line;
        while (IsSpace(*line)) ++line;
    }
    return ftp_callee::Atoi(line);
}
