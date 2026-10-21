// FTPClient.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of FTPClient code, named after it.

#include "analyser.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "helpers/MslSupportHelpers.h"
#include "online.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "ui_ime.h"
#include "WinScrn.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <new>
#include <string>

// FTPClient (main app, OnlinePerson.cpp): unit v5101. RecvData 0x1020b59c, ~FTPClient 0x1020b46c, Connect 0x1020b730,
// _EnterPassiveMode 0x1020b904.
//
// The Mac object is 0x2773 bytes (a Connection/CSock base with the vtable at 0); the host's members keep the Mac offsets in the header's
// comments. The destructor's body is Destruct(flag): the Mac's hidden "delete storage" flag after `this`; the vtable stores are the
// compiler's here. ~FTPClient (the key function) calls Destruct(0).

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

// FTPClient::_Connect (0x1020bbe8) and FTPClient::_Download (0x1020c228), unit v5227.
// The Mac object derives from Connection (the control socket at 4, a sockaddr_in at 8, the receive buffer at 0x20); the host's FTPClient
// keeps those in mF04 (socket at +0, sockaddr_in at +4), and has no 0x400 byte receive buffer before mBuffer, so recv and ParseFileSize
// use mBuffer (the Mac passes this + 0x20 and this + 0x424).

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

// FTPClient members, unit v5273: the constructor body, Disconnect / _Disconnect, Download, _CloseDSocket, ParseFileSize.

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

// 0x1020c744: skips four fields (and the blanks after each) of a listing line, then reads the number (no end test: runs on past
// the end of a short line, like the original).
int32_t FTPClient::ParseFileSize(char* line) {
    for (int field = 0; field < 4; ++field) {
        while (!IsSpace(*line)) ++line;
        while (IsSpace(*line)) ++line;
    }
    return ftp_callee::Atoi(line);
}

// Unit v51070: FTPClient::_threadfn and small library functions of the Metrowerks runtime on the Mac's word layouts (main app).

// Placeholders for the data the IPBase constructor points the globals at (only the first words are known).
const int16_t gIPErrorCodesData[] = {0x2714, 0x2719, 0};   // 0x102b1804
const char* const gIPErrorTextsData[] = {nullptr};         // 0x102b186c
char gSockVtblData[8];                                     // 0x102b1954

namespace {
// The address of a global (outside the arena) as the Mac word the object stores: truncated.
inline uint32_t GAddr(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// 0x1020c860 (64): the worker thread: runs the loop, then wakes the second condition. Answers 0.
void* FTPClient::_threadfn(void* self) {
    FTPClient* client = static_cast<FTPClient*>(self);
    client->ProcessEvents();
    reinterpret_cast<bthread_cond*>(client->mCondB)->signal();
    return nullptr;
}

namespace v51070 {

// 0x1020ca80 (36)
void IPBaseCtor(uint32_t* self) {
    self[0] = GAddr(gIPSystemVtableBlock + 8);
    gIPErrorCodes = gIPErrorCodesData;
    gIPErrorTexts = gIPErrorTextsData;
}

// 0x1020cb7c (76)
uint32_t* IPBaseDtor(uint32_t* self, int16_t flag) {
    if (self) {
        self[0] = GAddr(gIPSystemVtableBlock + 8);
        if (flag > 0) v51070_callee::Delete(self);
    }
    return self;
}

// 0x1020db8c (312): erase(first, last) of a list of wide strings; the result iterator is stored at *result.
uint32_t* EraseWideStrings(uint32_t* result, uint32_t* list, uint32_t first, uint32_t last) {
    if (first == last) {
        *result = last;
        return result;
    }
    uint32_t* lastPrev = Words(Words(last)[0]);
    uint32_t* firstPrev = Words(Words(first)[0]);
    firstPrev[1] = lastPrev[1];
    uint32_t* after = Words(lastPrev[1]);
    after[0] = Words(first)[0];
    uint32_t node = first;
    while (node != last) {
        uint32_t* string = Words(node + 8);
        if (string != nullptr) {
            uint32_t* rep = Words(string[0]);
            if (rep[2] == 0) rep[2] = 1;
            uint32_t refs = rep[2] - 1;
            rep[2] = refs;
            if (refs != 0) string[0] = 0;
            if (string[0] != 0) {
                v51070_callee::Delete(Words(Words(string[0])[3]));
                v51070_callee::Delete(Words(string[0]));
            }
        }
        uint32_t current = node;
        node = Words(node)[1];
        v51070_callee::Delete(Words(current));
        list[0] = list[0] - 1;
    }
    *result = last;
    return result;
}

// 0x1020e2a0 (20)
void SockCtor(uint32_t* self) {
    self[0] = GAddr(gSockVtblData);
    self[1] = 0xffffffffu;
}

// 0x1020e2d4 (100)
Sock* SockDtor(Sock* self, int16_t flag) {
    if (self) {
        self->mVtable = GAddr(gSockVtblData);
        self->Close();
        if (flag > 0) v51070_callee::Delete(self);
    }
    return self;
}

// 0x1020e560 (92)
uint32_t* CTokenListDtor(uint32_t* self, int16_t flag) {
    if (self) {
        v51070_callee::CTokenListClear(self);
        if (flag > 0) v51070_callee::Delete(self);
    }
    return self;
}

// 0x1020eeec (96)
uint32_t* CTokenDtor(uint32_t* self, int16_t flag) {
    if (self) {
        v51070_callee::WStringDtor(self, -1);
        if (flag > 0) v51070_callee::Delete(self);
    }
    return self;
}

}  // namespace v51070

// online.cpp / OnlinePerson.cpp (main app), unit v51009:
//   VoteNotifyCallback 0x10213590, SwearFilter::Init 0x1020d1bc, stristr 0x1020cbec, IPBase::ShowError 0x1020cac8,
//   FTPClient::ProcessEvents 0x1020c8cc.

// 0x1020c8cc. The result keeps its value from the pass before when a state does nothing (the original's register).
void FTPClient::ProcessEvents() {
    bool stop = false;
    int32_t result = 0;
    int32_t& control = *reinterpret_cast<int32_t*>(mF04);   // Mac 0x04: the control socket
    while (!stop) {
        ftp_callee::CondWait(mCondA, -1);
        const int32_t state = mF276c;
        mReady = 1;
        if (state == 2) {
            if (control == -1) {
                result = 0x38;
            } else {
                ftp_callee::SendCommandLine(this, "QUIT\r\n");
                const int32_t code = ftp_callee::RecvResponse(this, nullptr, 0, true);
                if (code == 0 || code == 0xdd) {
                    ftp_callee::SockClose(this);
                    if (mListenSocket != -1) {
                        ftp_callee::Shutdown(mListenSocket, 1);
                        while (ftp_callee::Recv(mListenSocket, mBuffer, 0x400, 0) > 0) {
                        }
                        ftp_callee::CloseSocket(mListenSocket);
                        mListenSocket = -1;
                    }
                    result = 0;
                } else {
                    result = 0x43;
                }
            }
        } else if (state < 2) {
            if (state == 0) {
                stop = true;
                result = 0;
            } else if (state > 0) {
                result = _Connect();
            }
        } else if (state < 4) {
            result = _Download();
        }
        ftp_callee::ControlCallback(mControl, state, result);
    }
}
