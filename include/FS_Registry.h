// FS_Registry.cpp (main app 0x100038b0..0x10003fa4; src/FS_Registry.cpp): the game's
// folders and its language, read at start-up (the Windows version kept them in the
// registry). Results are TbErrors, returned through a hidden r3 buffer on PowerPC.
// OnlineConfig's destructor (0x10003508) is in this range; it is implicit here (the
// members are std::strings).
#pragma once
#include <cstdint>

#include "TbDynamicStringTemplate.h"
#include "TbError.h"

// 0x100038b0: the Windows language id of a language bit (1 English 0x809, 2 French 0x40c,
// 4 German 7, 8 Spanish 0xa, 0x10 Swedish 0x1d, 0x20 Italian 0x10, 0x40 Dutch 0x13, 0x80
// Polish 0x15, 0x100 Danish 6, 0x200 Japanese 0x11, 0x400 traditional Chinese 0x404, 0x800
// simplified Chinese 0x804, 0x1000 Thai 0x41e, 0x2000 Korean 0x412, 0x4000 Hebrew 0x40d,
// 0x8000 American 0x409; anything else 0x809).
uint32_t TPConvertTPLangDefs(uint32_t language);
TbError FS_ReadRegistry();           // 0x10003a2c: the project, data, level and language paths
// 0x10003b64: the language is the first entry of the project's data:language folder that
// names one of the 16 languages (English otherwise).
TbError FS_ReadLanguageRegistry();

// Globals of FS_Registry.cpp (original names unknown).
extern char gLanguageName[0x400];    // 0x102b2e18, the language's name (CText::GetLanguageName)
// The strings __sinit_FS_Registry_cpp constructs (the same set as TPMain.cpp's);
// FS_ReadLanguageRegistry matches the language folder's entries with these names.
extern TbDynamicStringTemplate<char> gFSRegistryLanguages[16];   // 0x102b3258
extern TbDynamicStringTemplate<char> gFSRegistryTitle;           // 0x102b3238
extern TbDynamicStringTemplate<char> gFSRegistryLanguage;        // 0x102b3218
