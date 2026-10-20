// Connection::RecvResponse and Connection::GetLocalIPAddress (main app, email.cpp).
#include <cstdint>

#include "FS_Online.h"

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
