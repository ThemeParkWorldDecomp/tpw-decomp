// FTPClient::_Connect (0x1020bbe8) and FTPClient::_Download (0x1020c228), unit v5227.
// The Mac object derives from Connection (the control socket at 4, a sockaddr_in at 8, the receive buffer at 0x20); the host's FTPClient
// keeps those in mF04 (socket at +0, sockaddr_in at +4), and has no 0x400 byte receive buffer before mBuffer, so recv and ParseFileSize
// use mBuffer (the Mac passes this + 0x20 and this + 0x424).
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "FS_Online.h"

namespace {

struct SockAddrIn {
    uint16_t family;
    uint16_t port;
    uint32_t ip;
    uint8_t zero[8];
};

inline int32_t& ControlSocket(FTPClient* c) { return *reinterpret_cast<int32_t*>(c->mF04); }
inline SockAddrIn* ControlAddr(FTPClient* c) { return reinterpret_cast<SockAddrIn*>(c->mF04 + 4); }

// The data connection of _Download's two identical blocks: 0 or 0x40.
int32_t OpenDataSocket(FTPClient* c) {
    SockAddrIn local;
    c->mDataSocket = ftp_callee::Socket(2, 1, 0);
    if (c->mServer.mRep->mLength != 0) {
        std::memset(&local, 0, 0x10);
        local.family = 2;
        local.ip = ftp_callee::InetAddr(c->mServer.mRep->mData);
        ftp_callee::Bind(c->mDataSocket, &local, 0x10);
    }
    local.ip = c->mIpWord;
    local.family = 2;
    local.port = c->mPort;
    return ftp_callee::SockConnect(c->mDataSocket, &local, 0x10) != 0 ? 0x40 : 0;
}

// Shut the data connection down, read until it ends, close it.
void CloseData(FTPClient* c) {
    ftp_callee::Shutdown(c->mDataSocket, 1);
    while (ftp_callee::Recv(c->mDataSocket, c->mBuffer, 0x400, 0) > 0) {
    }
    ftp_callee::CloseSocket(c->mDataSocket);
    c->mDataSocket = -1;
}

}  // namespace

// 0x1020bbe8
int32_t FTPClient::_Connect() {
    int32_t error;
    char line[0x100];
    ControlSocket(this) = ftp_callee::Socket(2, 1, 0);
    if (ControlSocket(this) == -1) {
        error = 0x34;
        goto fail;
    }
    if (mServer.mRep->mLength != 0) {
        SockAddrIn* sa = ControlAddr(this);
        std::memset(sa, 0, 0x10);
        sa->family = 2;
        sa->ip = ftp_callee::InetAddr(mServer.mRep->mData);
        ftp_callee::Bind(ControlSocket(this), sa, 0x10);
    }
    {
        uint32_t address = ftp_callee::InetAddr(mStrings);
        if (address == 0xffffffffu && std::strcmp(mStrings, "255.255.255.255") != 0) {
            FTPHostent* host = ftp_callee::GetHostByName(mStrings);
            if (host != nullptr) {
                address = *host->mAddrList[0];
            }
        }
        SockAddrIn* sa = ControlAddr(this);
        sa->ip = address;
        sa->family = 2;
        sa->port = 0x15;
    }
    if (ftp_callee::SockConnect(ControlSocket(this), ControlAddr(this), 0x10) != 0) {
        error = 0x40;
        goto fail;
    }
    {
        int32_t code = ftp_callee::RecvResponse(this, nullptr, 0, true);
        if (code == 0) {
            error = 0x38;
            goto fail;
        }
        if (code != 0xdc) {
            error = 0x41;
            goto fail;
        }
        std::sprintf(line, "USER %s\r\n", mStrings + 0x60);
        ftp_callee::SendCommandLine(this, line);
        code = ftp_callee::RecvResponse(this, nullptr, 0, true);
        if (code == 0) {
            error = 0x38;
            goto fail;
        }
        if (static_cast<uint32_t>(code - 0x14b) <= 1u) {
            std::sprintf(line, "PASS %s\r\n", mStrings + 0xc0);
            ftp_callee::SendCommandLine(this, line);
            code = ftp_callee::RecvResponse(this, nullptr, 0, true);
            if (code == 0) {
                error = 0x38;
                goto fail;
            }
            if (code != 0xe6) {
                error = 0x42;
                goto fail;
            }
        }
        ftp_callee::SendCommandLine(this, "TYPE I\r\n");
        code = ftp_callee::RecvResponse(this, nullptr, 0, true);
        if (code == 0) {
            error = 0x38;
            goto fail;
        }
        if (code != 200) {
            error = 0x43;
            goto fail;
        }
        return 0;
    }
fail:
    ftp_callee::SockClose(this);
    ftp_callee::CloseDSocket(this, &mListenSocket);
    return error;
}

// 0x1020c228
int32_t FTPClient::_Download() {
    int32_t fileSize = -1;   // r30
    int32_t error;           // r28
    char line[0x100];
    int32_t code;
    if (ControlSocket(this) == -1) return 0x38;
    error = _EnterPassiveMode();
    if (error != 0) goto fail;
    if (mListenSocket == -1 && mDataSocket == -1) {
        error = OpenDataSocket(this);
        if (error != 0) goto fail;
    }
    mDone = 0;
    mTotal = 0;
    std::sprintf(line, "LIST %s\r\n", mPath + 0x100);
    ftp_callee::SendCommandLine(this, line);
    code = ftp_callee::RecvResponse(this, nullptr, 0, true);
    if (code == 0) {
        error = 0x38;
        goto fail;
    }
    if (code == 0x7d || code == 0x96) {
        error = 1;
        if (RecvData(false, -1) == 0) {
            fileSize = ftp_callee::ParseFileSize(this, reinterpret_cast<char*>(mBuffer));
        } else {
            error = 0;
        }
        code = ftp_callee::RecvResponse(this, nullptr, 0, true);
        if (code == 0) {
            error = 0x38;
            goto fail;
        }
        if (code != 0xfa && code != 0xe2) {
            error = 0x44;
            goto fail;
        }
        if (code == 0xe2) {
            if ((error & 0xff) == 0) {
                error = 0x44;
                goto fail;
            }
            if (mDataSocket != -1) CloseData(this);
            error = _EnterPassiveMode();
            if (error != 0) goto fail;
            if (mListenSocket == -1 && mDataSocket == -1) {
                error = OpenDataSocket(this);
                if (error != 0) goto fail;
            }
        }
    }
    mTotal += fileSize;
    std::sprintf(line, "RETR %s\r\n", mPath + 0x100);
    ftp_callee::SendCommandLine(this, line);
    code = ftp_callee::RecvResponse(this, nullptr, 0, true);
    if (code == 0) {
        error = 0x38;
        goto fail;
    }
    if (code != 0x96 && code != 0x7d) {
        error = 0x43;
        goto fail;
    }
    RecvData(true, fileSize);
    code = ftp_callee::RecvResponse(this, nullptr, 0, true);
    if (code == 0) {
        error = 0x38;
        goto fail;
    }
    if (code != 0xe2 && code != 0xfa) {
        error = 0x44;
        goto fail;
    }
    if (code == 0xe2 && mDataSocket != -1) CloseData(this);
    return 0;
fail:
    if (mDataSocket != -1) CloseData(this);
    return error;
}
