// FTPClient (main app, OnlinePerson.cpp): unit v5101. RecvData 0x1020b59c, ~FTPClient 0x1020b46c, Connect 0x1020b730,
// _EnterPassiveMode 0x1020b904.
//
// The Mac object is 0x2773 bytes (a Connection/CSock base with the vtable at 0); the host's members keep the Mac offsets in the header's
// comments. The destructor's body is Destruct(flag): the Mac's hidden "delete storage" flag after `this`; the vtable stores are the
// compiler's here. ~FTPClient (the key function) calls Destruct(0).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "FS_Online.h"

using namespace ftp_callee;

FTPClient::~FTPClient() { Destruct(0); }

// 0x1020b46c
FTPClient* FTPClient::Destruct(int16_t deleteFlag) {
    while (mReady == 0) Sleep(1);
    mReady = 0;
    mF276c = 0;
    CondSignal(&mCondA);
    if (mThreadStarted != 0) CondWait(&mCondB, -1);
    WSACleanup();
    CondDtor(&mCondB, -1);
    CondDtor(&mCondA, -1);
    StringDtor(&mServer, -1);
    CSockDtor(this, 0);
    if (deleteFlag > 0) OperatorDelete(this);
    return this;
}

// 0x1020b59c
int32_t FTPClient::RecvData(bool save, int32_t count) {
    uint8_t* at = mBuffer;
    int32_t received = 0;
    void* file = nullptr;
    int32_t result = 0;
    uint8_t addr[16];
    int32_t addrLength = 0x10;
    if (mListenSocket != -1 && mDataSocket == -1) {
        mDataSocket = Accept(mListenSocket, addr, &addrLength);
        if (mDataSocket == -1) return 3;
    }
    if (save) {
        file = Fopen(mPath, "wb");
        if (file == nullptr) result = 1;   // the loop still runs, without a file
    }
    while (received != count) {
        int32_t n = Recv(mDataSocket, at, 0x1fff, 0);
        if (n <= 0) {
            result = 2;
            break;
        }
        if (file != nullptr) Fwrite(mBuffer, 1, n, file);
        received += n;
        mDone += n;
        if (!save) {
            at += n;
            *at = 0;
            if (std::strchr(reinterpret_cast<char*>(mBuffer), 10) != nullptr) received = count;
        }
    }
    if (file != nullptr) Fclose(file);
    return result;
}

// 0x1020b730
int32_t FTPClient::Connect(char* a, char* b, char* c, char* server) {
    char* first = mStrings;
    char* second = mStrings + 0x60;
    char* third = mStrings + 0xc0;
    std::strncpy(first, a, 0x60);
    std::strncpy(second, b, 0x60);
    std::strncpy(third, c, 0x60);
    for (char* p = second; *p != 0; ++p) {
        if (*p == 0xd || *p == 0xa) {
            *p = 0;
            break;
        }
    }
    for (char* p = third; *p != 0; ++p) {
        if (*p == 0xd || *p == 0xa) {
            *p = 0;
            break;
        }
    }
    if (server != nullptr) StringAssign(&mServer, server, static_cast<uint32_t>(std::strlen(server)));
    if (mThreaded == 0) return _Connect();
    if (mThreadStarted == 0) {
        if (BthreadCreate(&FTPClient::_threadfn, this) != -1) mThreadStarted = 1;
    }
    while (mReady == 0) Sleep(1);
    mReady = 0;
    mF276c = 1;
    CondSignal(&mCondA);
    return 0;
}

// 0x1020b904
int32_t FTPClient::_EnterPassiveMode() {
    int32_t nameLength = 0x10;
    char response[0x100];
    SendCommandLine(this, "PASV\r\n");
    int32_t code = RecvResponse(this, response, 0x100, true);
    if (code == 0) return 0x38;
    mFamily = 2;
    if (code == 0xe3) {
        char* p = std::strchr(response, '(');
        if (p == nullptr) return 0x43;
        p += 1;
        mIp[0] = static_cast<uint8_t>(std::atoi(p));
        p = std::strchr(p, ',') + 1;
        mIp[1] = static_cast<uint8_t>(std::atoi(p));
        p = std::strchr(p, ',') + 1;
        mIp[2] = static_cast<uint8_t>(std::atoi(p));
        p = std::strchr(p, ',') + 1;
        mIp[3] = static_cast<uint8_t>(std::atoi(p));
        p = std::strchr(p, ',') + 1;
        mPort = static_cast<uint16_t>((std::atoi(p) << 8) & 0xff00);
        p = std::strchr(p, ',') + 1;
        mPort = static_cast<uint16_t>(mPort | static_cast<uint16_t>(std::atoi(p)));
        return 0;
    }
    mListenSocket = Socket(2, 1, 0);
    if (mServer.mRep->mLength != 0) {
        mIpWord = InetAddr(mServer.mRep->mData);
    } else {
        mIpWord = GetLocalIPAddress(this);
    }
    mPort = 0;
    if (Bind(mListenSocket, &mFamily, 0x10) != 0) return 0x34;
    if (Listen(mListenSocket, 5) != 0) return 0x34;
    GetSockName(mListenSocket, &mFamily, &nameLength);
    std::sprintf(response, "PORT %d,%d,%d,%d,%d,%d\r\n", mIp[0], mIp[1], mIp[2], mIp[3], (mPort >> 8) & 0xff, mPort & 0xff);
    SendCommandLine(this, response);
    code = RecvResponse(this, nullptr, 0, true);
    if (code == 0) return 0x38;
    if (code != 200) return 0x43;
    return 0;
}
