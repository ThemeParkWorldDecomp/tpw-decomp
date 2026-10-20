#include <cstdint>
#include <string>
#include "online.h"
#include "FS_Online.h"

// 0x1020e358: shut the socket down for writing, read until recv answers 0 or less, close it.
void Sock::Close() {
    if (mSocket != -1) {
        char buffer[256];
        ftp_callee::Shutdown(mSocket, 1);
        while (ftp_callee::Recv(mSocket, buffer, 0x100, 0) > 0) {
        }
        ftp_callee::CloseSocket(mSocket);
        mSocket = -1;
    }
}
