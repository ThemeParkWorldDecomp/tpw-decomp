// ccServerInfo::~ccServerInfo (0x101731dc, 220 bytes): frees the five strings (+0x08, +0x0c, +0x16, +0x1a, +0x22), each when not NULL, and
// clears the field after each. The Mac's destructor also takes the hidden "delete storage" short and calls operator delete when it is
// positive; the host's `delete` does that itself, so only the member work is here.
#include <cstdlib>

#include "FS_OnlineList.h"

ccServerInfo::~ccServerInfo() {
    if (mStr08.mOffset != 0) free(const_cast<char16_t*>(mStr08.get()));
    mStr08.mOffset = 0;
    if (mStr0C.mOffset != 0) free(const_cast<char16_t*>(mStr0C.get()));
    mStr0C.mOffset = 0;
    if (mStr16.mOffset != 0) free(const_cast<char16_t*>(mStr16.get()));
    mStr16.mOffset = 0;
    if (mStr1A.mOffset != 0) free(const_cast<char16_t*>(mStr1A.get()));
    mStr1A.mOffset = 0;
    if (mStr22.mOffset != 0) free(const_cast<char16_t*>(mStr22.get()));
    mStr22.mOffset = 0;
}
