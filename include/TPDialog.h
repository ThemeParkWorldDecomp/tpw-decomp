// TPDialog.cpp, main app: 0x10237950, 0x102379c8 and 0x10237a30.
#pragma once
#include <cstdint>
#include "TbDynamicStringTemplate.h"

uint32_t GetOptionsSetting();
int32_t GetNumKidSprites();
extern uint32_t gTPDialogOptions;  // 0x103bc664, original symbol name not recovered
// 0x103bc65c: the dialog's texture memory in megabytes (0: leave the engine's choice);
// EngineStartup_Dialog and InitialiseGameStage1 read it. Original name unknown.
extern uint32_t gTPDialogTextureMegabytes;
// 0x103bc660: the language bit FS_ReadLanguageRegistry found (nothing reads it). Original
// name unknown.
extern uint32_t gTPDialogLanguageBits;
extern TbDynamicStringTemplate<char> gTPDialogLanguages[16]; // 0x103bc6a8
extern TbDynamicStringTemplate<char> gTPDialogTitle;         // 0x103bc688
extern TbDynamicStringTemplate<char> gTPDialogLanguage;      // 0x103bc668
