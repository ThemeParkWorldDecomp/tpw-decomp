// LbFile_CompareFileStamps (bullfrog symbols shared 0x10000520, LbSCBack.cpp range): orders two directory listing time stamps.
// Unit w108 (the park's most recent save loader) uses it.
#include "LbFile.h"
#include "platform/macdoze.h"

// A NULL stamp is older than any, two NULLs are equal; else NS_MacDoze::CompareFileTime (the platform's; it compares to the
// second).
int32_t LbFile_CompareFileStamps(const TbFileStamp* a, const TbFileStamp* b) {
    if (a == nullptr && b == nullptr) return 0;
    if (a == nullptr) return -1;
    if (b == nullptr) return 1;
    return NS_MacDoze::CompareFileTime(reinterpret_cast<const NS_MacDoze::tagFILETIME*>(a),
                                       reinterpret_cast<const NS_MacDoze::tagFILETIME*>(b));
}
