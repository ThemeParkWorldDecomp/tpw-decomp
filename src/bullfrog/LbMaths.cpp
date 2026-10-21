// LbMaths.cpp: name proven by __sinit_LbMaths_cpp in the Bullfrog library.

#include "AudioPlaceHolder.h"
#include "bfl_FileSystem.h"
#include "bfl_Refpack.h"
#include "bfl_Strings.h"
#include "bfl_WadFile.h"
#include "bfl_ZLib.h"
#include "DrawState.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "LbSurface.h"
#include "platform/macdoze.h"
#include "TbFPUPrecision.h"
#include "WinScrn.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

// The bullfrog access chains and disc path.

namespace bfl::FileSystem {

CDiscFile::CDiscFile() {}
CDiscFile::~CDiscFile() {}
TbError CDiscFile::Open(const char* name, uint32_t mode) {
    TbDynamicStringTemplate<char> path(name);
    int32_t split = 20000;
    uint32_t start = 0xffffffff;
    bool found = false;
    while (split > 0 && !found) {
        split = path.FindReverseOneOf("\\/:", start);
        if (split > 0 && !NS_MacDoze::IsDBCSLeadByte(static_cast<uint8_t>(path.mData[split - 1]))) {
            found = true;
            ++split;
        } else start = static_cast<uint32_t>(split - 1);
    }
    if (split == -1) split = 0;
    TbDynamicStringTemplate<char> basename(path.Mid(static_cast<uint32_t>(split)));
    path = path.Left(static_cast<uint32_t>(split));
    CDiscFileStorage storage;
    if (!storage.Open(path.mData).Succeeded()) return TbError(-1);
    if (!storage.OpenFile(*this, basename.mData, mode, nullptr).Succeeded()) return TbError(-1);
    return TbError(0);
}

}

// Bullfrog LbSCBack.cpp file access and wide storage wrappers, and LbMaths.cpp CMemFile::Open.

namespace bfl::FileSystem {

TbError CMemFile::Open(uint32_t size) { // 0x1002951c
    if (mImplementation != nullptr) return TbError(-1);
    auto* access = new (std::nothrow) _TbMemFileAccess(size);
    AttachAccessClass(access);
    return TbError(0);
}

}

// bullfrog TbSystemInfo.cpp, LbDraw.cpp, WinScrn.cpp, LbMaths.cpp, TbJPEGSaver.cpp, Draw.cpp, LbSCBack.cpp small functions (unit v51071).

// ---- static initialisers
void __sinit_LbMaths_cpp() { gLbMathsAtanScale = 1024.0 / gMathLibPi; }   // 0x1002971c
