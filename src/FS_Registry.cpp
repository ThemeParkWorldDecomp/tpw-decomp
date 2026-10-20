// FS_Registry.cpp (decompiled from the main app of SimTheme Park 2.04b3: 0x100038b0..0x10003cf4
// and __sinit_FS_Registry_cpp at 0x10003d28). The Windows version read these settings from
// the registry; the Mac version derives them from the project directory.
//
// OnlineConfig::~OnlineConfig (0x10003508) is emitted in this range; here it is the
// implicit destructor of OnlineConfig's std::string members (FS_Online.h).
#include "FS_Registry.h"

#include <cstring>

#include "FS_System.h"
#include "LbFile.h"
#include "TPDialog.h"
#include "TPGlobals.h"
#include "Text.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"

char gLanguageName[0x400];   // 0x102b2e18

// __sinit_FS_Registry_cpp, in construction order: the same strings as TPMain.cpp's
// gTPMainLanguages (each file has its own copies).
TbDynamicStringTemplate<char> gFSRegistryLanguages[16] = {   // 0x102b3258, 20-byte slots
    TbDynamicStringTemplate<char>("English"),
    TbDynamicStringTemplate<char>("French"),
    TbDynamicStringTemplate<char>("German"),
    TbDynamicStringTemplate<char>("Spanish"),
    TbDynamicStringTemplate<char>("Swedish"),
    TbDynamicStringTemplate<char>("Italian"),
    TbDynamicStringTemplate<char>("Dutch"),
    TbDynamicStringTemplate<char>("Polish"),
    TbDynamicStringTemplate<char>("Danish"),
    TbDynamicStringTemplate<char>("Japanese"),
    TbDynamicStringTemplate<char>("TraditionalChinese"),
    TbDynamicStringTemplate<char>("SimplifiedChinese"),
    TbDynamicStringTemplate<char>("Thai"),
    TbDynamicStringTemplate<char>("Korean"),
    TbDynamicStringTemplate<char>("Hebrew"),
    TbDynamicStringTemplate<char>("American")
};
TbDynamicStringTemplate<char> gFSRegistryTitle("Theme Park World");    // 0x102b3238
TbDynamicStringTemplate<char> gFSRegistryLanguage("Language");         // 0x102b3218

uint32_t TPConvertTPLangDefs(uint32_t language) {
    switch (language) {
    case 0x1: return 0x809;
    case 0x2: return 0x40c;
    case 0x4: return 0x7;
    case 0x8: return 0xa;
    case 0x10: return 0x1d;
    case 0x20: return 0x10;
    case 0x40: return 0x13;
    case 0x80: return 0x15;
    case 0x100: return 0x6;
    case 0x200: return 0x11;
    case 0x400: return 0x404;
    case 0x800: return 0x804;
    case 0x1000: return 0x41e;
    case 0x2000: return 0x412;
    case 0x4000: return 0x40d;
    case 0x8000: return 0x409;
    default: return 0x809;
    }
}

TbError FS_ReadRegistry() {
    gTPFileInfo.SetLocalPath(TbDynamicStringTemplate<char>(LbFile_GetProjectDir()));
    gTPFileInfo.SetDataPath(TbDynamicStringTemplate<char>(gTPGlobals.mDataFolder));
    gTPFileInfo.SetLevelPath(TbDynamicStringTemplate<char>("levels"));
    gTPFileInfo.SetLanguagePath(TbDynamicStringTemplate<char>(gTPGlobals.mDefaultLanguagePath));
    return TbError(0);
}

TbError FS_ReadLanguageRegistry() {
    TbDynamicStringTemplate<char> pattern(LbFile_GetProjectDir());
    FS_AddDirectorySeparator(pattern);
    pattern += gTPGlobals.mLanguageFolder;
    FS_AddDirectorySeparator(pattern);
    pattern += "*";
    gTPDialogLanguageBits = 1;   // English unless the folder names another language
    NS_MacDoze::_WIN32_FIND_DATAA found;
    NS_MacDoze::InternalHandle* search = NS_MacDoze::FindFirstFile(pattern.mData, &found);
    if (search != reinterpret_cast<NS_MacDoze::InternalHandle*>(-1)) {   // INVALID_HANDLE_VALUE
        unsigned char name[256];
        unsigned char language[256];
        SamsUtilities::UString::c2pcpy(name, found.cFileName);
        for (uint32_t i = 0; i < 16; ++i) {
            SamsUtilities::UString::c2pcpy(language, gFSRegistryLanguages[i].mData);
            if (EqualString(name, language, false, false)) {
                gTPDialogLanguageBits = 1u << i;
                break;
            }
        }
        NS_MacDoze::FindClose(search);
    }
    const uint32_t id = TPConvertTPLangDefs(gTPDialogLanguageBits);
    const uint32_t bits = gTPDialogLanguageBits;   // never 0
    int32_t i = 0;
    while (((1u << i) & bits) == 0) ++i;
    std::strcpy(gLanguageName, gFSRegistryLanguages[i].mData);
    gText.SetLanguageId(id);
    return TbError(0);
}
