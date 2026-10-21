// TPGlobals.cpp: name proven by __sinit_TPGlobals_cpp in the main application.

#include "FS_Online.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "MslWStringTree.h"
#include "online.h"
#include "std_basic_filebuf.h"
#include "std_basic_streambuf_wchar.h"
#include "std_deque_errorthingy.h"
#include "tb_ezsprite.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
#include "TPGlobals.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"
#include "UI_pump.h"
#include <cstdint>
#include <cstring>
#include <new>

// TPGlobals.cpp (SimTheme Park 2.04b3): the game's global names: folders, file names,
// extensions and patterns, as 159 bullfrog strings built by one constructor (0x102328bc,
// 2,656 bytes of constructor calls), with the sine table and a setting before them. The
// values are the constructor's string constants (two pools: char at 0x10251aee, wchar_t at
// 0x10252020), in its order. Spellings are the original's ("Langauge").

TPGlobals::TPGlobals()
    : mSinTableSize(0), mSinTableMask(0), mSinTableScale(0.0f), mSinTable{}, mEngineFlags(0), mF4010(0), mF4014(0.0f), mF4018(0), mPad401C{},
      mF4024("TPWorld"),
      mF4038("TPWMutex"),
      mSeparator(":"),
      mSearchPath(""),
      mCurrentFolder("."),
      mF4088("data:levels"),
      mLevelsFolder("levels"),
      mF40B0("save"),
      mF40C4("users"),
      mAllFiles("*"),
      mF40EC("hoardings.sam"),
      mF4100("endhoardings.sam"),
      mSoundBalanceFile("sound.sam"),
      mOnlineBalanceFile("online.sam"),
      mF413C("onlineoverride.sam"),
      mF4150("cat_ride"),
      mF4164("cat_rideBANK.map"),
      mF4178("cat_rideSFX.map"),
      mF418C("BankHD.sdt"),
      mLowOptionsFile("low.sam"),
      mMediumOptionsFile("med.sam"),
      mHighOptionsFile("high.sam"),
      mF41DC("custom.sam"),
      mResolutionBalanceFile("Resolution.sam"),
      mF4204("standard.sam"),
      mF4218("online_standard.sam"),
      mF422C("online_rides.sam"),
      mSamExtension(".sam"),
      mF4254(".sam"),
      mOnlineSamExtension(".sam"),
      mF427C("Easy_"),
      mOnlinePrefix("Online_"),
      mF42A4("coaster"),
      mF42B8(".txt"),
      mF42CC(".md2"),
      mF42E0(".tps"),
      mF42F4(".tpc"),
      mF4308(".fps"),
      mF431C(".fpc"),
      mF4330(".ffn"),
      mF4344(".lan"),
      mF4358(".tgq"),
      mF436C(".tga"),
      mF4380(".adt"),
      mWadExtension(".wad"),
      mF43A8(".emt"),
      mF43BC(".map"),
      mF43D0(".sdt"),
      mWadPattern(".wad*"),
      mWadFolderPattern(".wad*:"),
      mSaveExtension(".TPWS"),
      mF4420(u".TPWS"),
      mF4434(".TPWI"),
      mF4448(u".TPWI"),
      mSaveFilePattern(".TPW*"),
      mF4470(u".TPW*"),
      mF4484(".LAYS"),
      mF4498(u".LAYS"),
      mF44AC(u".INTS"),
      mWideSeparator(u":"),
      mSaveFolder(u"save"),
      mUsersFolder(u"users"),
      mOnlineFolder(u"online"),
      mAutosaveName(u"autosave"),
      mRestartName(u"restart"),
      mRefreshName(u"Refresh"),
      mConfigFile(u"Config.tcf"),
      mF4560(u"Dialog.tcf"),
      mF4574("*.md2"),
      mF4588("*.ESP"),
      mF459C("*.tga"),
      mF45B0("*.emt"),
      mF45C4("Levels"),
      mF45D8("Fonts"),
      mF45EC("Movies"),
      mF4600("UI"),
      mF4614("SharedTex"),
      mF4628("Small_SharedTex"),
      mF463C("GlobalAdvisor"),
      mF4650("Rides"),
      mF4664("Sprites"),
      mF4678("Langauge"),
      mF468C("Generic"),
      mF46A0("Postcard"),
      mF46B4("2DMap"),
      mF46C8("Terrain"),
      mF46DC("Data"),
      mF46F0("Lobby"),
      mF4704("Upgrades"),
      mF4718("Shops"),
      mF472C("Sideshow"),
      mF4740("Features"),
      mF4754("Lips"),
      mF4768("GlobalSounds"),
      mF477C("GlobalSpeech"),
      mF4790("LocalSounds_"),
      mF47A4("LocalSpeech_"),
      mF47B8("LocalMusic_"),
      mF47CC("LocalAdvisor"),
      mF47E0("Dynamic"),
      mF47F4("Hoarding"),
      mF4808("MiscMesh"),
      mF481C("Queue"),
      mDataFolder("data"),
      mF4844("data.wad"),
      mF4858("data:Lobby"),
      mF486C("data:Lobby.wad"),
      mF4880("data:fonts"),
      mF4894("tpwfnt"),
      mF48A8("data:movies"),
      mF48BC("data:ui"),
      mF48D0("data:ui.wad"),
      mF48E4("data:esprites"),
      mF48F8("data:esprites.wad"),
      mF490C("data:generic"),
      mF4920("data:generic.wad"),
      mLanguageFolder("data:language"),
      mF4948("data:language.wad"),
      mF495C("data:2DMap"),
      mF4970("data:2DMap.wad"),
      mF4984("data:Postcard"),
      mF4998("data:Postcard.wad"),
      mF49AC("Terrain"),
      mF49C0("Terrain.wad"),
      mF49D4("Dynamic"),
      mF49E8("Dynamic.wad"),
      mF49FC("Hoarding"),
      mF4A10("Hoarding.wad"),
      mF4A24("MiscMesh"),
      mF4A38("MiscMesh.wad"),
      mF4A4C("Queue"),
      mF4A60("Queue.wad"),
      mF4A74("data:global:speech:lips"),
      mF4A88("data:global:speech:lips.wad"),
      mF4A9C("data:global:sound"),
      mF4AB0("data:global:sound.wad"),
      mF4AC4("data:global:speech"),
      mF4AD8("data:global:speech.wad"),
      mF4AEC("data:global:advisor"),
      mF4B00("data:global:advisor.wad"),
      mDefaultLanguagePath("language:default"),
      mF4B28("Advisor"),
      mF4B3C("Advisor.wad"),
      mF4B50("Coasters"),
      mF4B64("Rides:Coasters"),
      mF4B78("ShareTex"),
      mF4B8C("ssharete"),
      mF4BA0("Sky"),
      mF4BB4("Rides"),
      mF4BC8("Sideshow"),
      mF4BDC("Shops"),
      mF4BF0("Features"),
      mF4C04("Upgrades"),
      mF4C18("sound"),
      mF4C2C("sound.wad"),
      mF4C40("speech"),
      mF4C54("speech.wad"),
      mF4C68("music"),
      mF4C7C("music.wad") {}

// Lead 2026-10-20 (v5L02): the destructor 0x10233378 is src/TPGlobals_Dtor_TPGlobals.cpp (v5692).

// The object, gTPGlobals, is defined in GlobalOrder.cpp (construction order matters).

// Unit v51058: twenty small functions of the main app: the world lobby controller's messages, the static initialisers of
// ui_worldLobbyController.cpp, ui_publishParkPanel.cpp, TPGlobals.cpp and OnlinePerson.cpp, the 2D map callback, two lobby screen
// callbacks that call a virtual slot, a few std::__cdeque / std::less helpers.

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
uint8_t gTPGlobalsDtorVector[8];                       // stand-in for the TVector at 0x10268a08 (~TPGlobals)
uint8_t gTPGlobalsNode[12];                            // 0x103bc158
// ---- TPGlobals.cpp and OnlinePerson.cpp
// 0x1023331c
void __sinit_TPGlobals_cpp() {
    TPGlobals* globals = ::new (static_cast<void*>(&gTPGlobals)) TPGlobals();
    __register_global_object(globals, gTPGlobalsDtorVector, gTPGlobalsNode);
}

// 0x10233378 TPGlobals::~TPGlobals (2624 bytes).
// The original calls the string destructors of the 159 members one by one from the last (0x4c7c) to the first
// (0x4024), then, when the hidden delete flag (a short, r4) is > 0, operator delete(this). The member order of the
// class is the Mac's offset order, so the implicit member destruction runs the same destructors in the same order.
// The null check and the delete flag belong to the deleting variant, which the C++ compiler generates.

TPGlobals::~TPGlobals() {}

// Global objects whose constructors read other global objects, defined in ONE file in the
// order the original's static initialisers run. C++ fixes the order of construction
// within a file and leaves it open between files; the original relied on its link order.
//
// gTPGlobals (__sinit_TPGlobals_cpp, 0x1023331c; object at 0x103b74c8): the default names.
// gGame (__sinit_Game_cpp, 0x1014b118; object at 0x1037f134): its constructor copies one of
// those names.

TPGlobals gTPGlobals;
CGame gGame;
