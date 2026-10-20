// String helpers of the main app (decompiled from SimTheme Park 2.04b3:
// 0x10005354..0x1000585c). The original file is unknown (see TPString.h).
#include "TPString.h"

#include "LbFile.h"
#include "platform/macdoze.h"

bool TPStrGtr(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b) {
    return NS_MacDoze::_stricmp(a.mData, b.mData) > 0;
}

bool TPStrEq(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b) {
    return NS_MacDoze::_stricmp(a.mData, b.mData) == 0;
}

bool TPUniStrEq(const TbDynamicStringTemplate<char16_t>& a, const TbDynamicStringTemplate<char16_t>& b) {
    return NS_MacDoze::_wcsicmp(a.mData, b.mData) == 0;
}

TbDynamicStringTemplate<char>& RemoveExtension(const TbDynamicStringTemplate<char>& extension,
                                                TbDynamicStringTemplate<char>& name) {
    const TbDynamicStringTemplate<char> tail(name.Right(static_cast<uint32_t>(extension.Length())));
    if (NS_MacDoze::_stricmp(tail.mData, extension.mData) == 0)
        name = name.Left(static_cast<uint32_t>(name.Length() - extension.Length()));
    return name;
}

int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    int32_t written = 0;
    const int32_t size = NS_MacDoze::WideCharToMultiByte(0, 0, in.mData, -1, nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        char* text = static_cast<char*>(LbMem_Alloc(static_cast<uint32_t>(size)));
        if (text != nullptr) {
            written = NS_MacDoze::WideCharToMultiByte(0, 0, in.mData, -1, text, size, nullptr, nullptr);
            out = text;
            LbMem_Free(text);
        }
    }
    return written;
}

int32_t StringToUniString(TbDynamicStringTemplate<char16_t>& out, const TbDynamicStringTemplate<char>& in) {
    int32_t written = 0;
    const int32_t size = NS_MacDoze::MultiByteToWideChar(0, 0, in.mData, -1, nullptr, 0);
    if (size > 0) {
        char16_t* text = static_cast<char16_t*>(LbMem_Alloc(static_cast<uint32_t>(size) * sizeof(char16_t)));
        if (text != nullptr) {
            written = NS_MacDoze::MultiByteToWideChar(0, 0, in.mData, -1, text, size);
            out = text;
            LbMem_Free(text);
        }
    }
    return written;
}
