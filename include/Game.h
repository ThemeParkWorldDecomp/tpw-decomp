// Game.cpp (main app): CGame and the command line. PARTIAL; untranslated.
#pragma once
#include <cstdint>

#include "TbTList.h"
#include "TbDynamicStringTemplate.h"

class CTPResourceManager;

class CSkyObjects;

// The game (Game.cpp; 0x40 bytes on the Mac). One object, gGame, built by the file's
// static initialiser (0x1014b118), which is where the constructor's code is.
class CGame {
public:
    enum ePauseFlag : int32_t {};
    CGame();                                    // inline in __sinit_Game_cpp, 0x1014b118
    ~CGame();                                   // 0x10147d5c
    // The three sound resources of every theme found by FS_ScanThemes.
    void InitLocalSoundResources();             // 0x10148468
    // Registers the fourteen resources the game always has (and the themes' sounds).
    void InitResources();                       // 0x10148730
    void Pause(ePauseFlag flag, bool allSounds); // 0x10149904
    void Unpause();                             // 0x101499d0
    // 0x10149a74 (untranslated, 212 bytes; the pause window's key handler calls it with false).
    void TogglePause(bool allSounds);
    int32_t GetCurrentTheme() const;            // 0x10149b70
    // 0x10149ba8, 0x10149c7c, 0x1014a08c (src/game/Game.cpp): the index (or the name, ignoring case) of the current theme, which
    // the master save follows; SetCurrentThemeName copies the name of theme number mCurrentTheme out of mThemes.
    void SetCurrentTheme(int32_t theme);
    void SetCurrentTheme(const TbDynamicStringTemplate<char>& name);
    void SetCurrentThemeName();
    // Adds a copy of `name` to the themes, kept in order (ignoring case).
    void AddThemeName(const TbDynamicStringTemplate<char>& name);       // 0x10149e54
    void ClearThemeNames();                     // 0x10149fcc
    const TbDynamicStringTemplate<char>& GetCurrentThemeName();         // 0x1014a058
    // 0x1014a768 (src/game/Game.cpp): the names, in the player's language, of the two gates of the theme
    // whose unicode folder name is `theme` (the lobby's signs): the two sign texts of the theme's gate ride type from the ride
    // database. A theme that is none of the four leaves both names as the frame's uninitialised buffers hold them.
    void GetLocalisedThemeGateNames(const TbDynamicStringTemplate<char16_t>& theme, TbDynamicStringTemplate<char16_t>& gate1,
                                    TbDynamicStringTemplate<char16_t>& gate2) const;
    // 0x1014a570 (src/game/Game.cpp): sets `out` to the player's-language name of the theme whose
    // unicode folder name is `theme` (fantasy, hallow, jungle, space); any other theme leaves `out` alone.
    void GetLocalisedThemeName(const TbDynamicStringTemplate<char16_t>& theme, TbDynamicStringTemplate<char16_t>& out) const;
    // Unit v5064: the label of 0x1014a478 names the theme argument as a TbDynamicStringTemplate<char> (what GetCurrentThemeName answers);
    // CreatePublishParkPanel calls it that way.
    void GetLocalisedThemeName(const TbDynamicStringTemplate<char>& theme, TbDynamicStringTemplate<char16_t>& out) const;
    void InitThemeResources(const TbDynamicStringTemplate<char>& path); // 0x10148c3c
    int32_t InitWorld();                        // 0x10147e34: creates gGameWorld; nonzero on success
    // 0x10147ee4 (src/game/Game.cpp, unit w117): the park's theme and level: the ride database for the theme, the new level (the world's
    // SetupNewLevel), the quick save the command line names (its answer changes nothing) and the level's objects and settings.
    // 1 on success, 0 when LoadTheme, SetupNewLevel or InitLevel answered 0.
    int32_t InitTheme(char* path);
    int32_t LoadTheme(const char* path);        // 0x1014960c: gRideDatabase.LoadTheme(path), whose answer it returns (1 on success)
    int32_t InitLevel();                        // 0x10148028: InitLevel_Load, then InitLevel_Initialise when it answered nonzero; 0 or 1
    int32_t InitLevel_Load();                   // 0x1014809c: the theme's objects, sky, sky objects and the two hoardings tables; 0 when a table failed
    int32_t InitLevel_Initialise();             // 0x101482bc: the theme's fog, light, procedural texturing, advisor costumes and cell costs; 1
    // 0x101496ec (src/game/Game.cpp): the end of a level: the world is exited, freed and deleted, the ride database emptied, the sky closed, the
    // sky objects (when the display has flag 0x400000) and the message centre deleted, the theme's resources cleared. The Mac answers 1 (r3;
    // the callers ignore it).
    void ExitLevel();
    // 0x10149334 (src/game/Game.cpp): takes the current theme's resource and thirteen standing ones out of gGame's resource manager.
    void ClearThemeResources();
    void Iterate();                             // 0x10149838
    void DeInit();                              // 0x1014965c

    TbDynamicStringTemplate<char> mCurrentThemeName;        // 0x00  starts as gTPGlobals.mSearchPath ("")
    int32_t mCurrentTheme;                                  // 0x14  -1
    int32_t mRestart;                                       // 0x18  set when the player restarts the game (RestartCallback_OK)
    int32_t mF1C;                                           // 0x1c  game paused (0/1)
    uint8_t mF20;                                           // 0x20  last pause silenced all sounds
    TbPtrList<TbDynamicStringTemplate<char>> mThemes;       // 0x24  the themes' folder names, in order
    CTPResourceManager* mResourceManager;                   // 0x34
    CSkyObjects* mSkyObjects;                               // 0x38  NULL until a level makes them
    int32_t mF3C;   // 0x3c  1; GAMESYSTEM_Tick: 0 for the lobby and downloaded online parks, else 1
};
extern CGame gGame;   // 0x1037f134, original name unknown

// Unit w129: CGame::Iterate's function-local static (value 0x1037f130, guard byte 0x1037f12c): 1 from the first call, then whether bit 8 of gTPWFlags
// was set at the start of the last call (src/game/Game.cpp).
extern int32_t gGameIterateFlag;

// The four theme folder names the gate names compare with (Game.cpp's static initialiser builds them; the original names are
// unknown). Wide dynamic strings, 20 bytes each.
extern TbDynamicStringTemplate<char16_t> gGameThemeFantasy;   // 0x1037f10c "fantasy"
extern TbDynamicStringTemplate<char16_t> gGameThemeHallow;    // 0x1037f0ec "hallow"
extern TbDynamicStringTemplate<char16_t> gGameThemeJungle;    // 0x1037f0cc "jungle"
extern TbDynamicStringTemplate<char16_t> gGameThemeSpace;     // 0x1037f0ac "space"

// 0x1014a10c (src/game/Game.cpp): makes `theme` (an old theme number of the settings file) the current theme (ends in
// CFSGlobalMasterSave::ChangeCurrentTheme).
void ConverttoCurrentTheme(int32_t theme);

// The command line (Game.cpp range; 0x208 bytes on the Mac, constructed by
// __sinit_GameSys_cpp into GameSys.cpp's gCommandLine).
class CCommandLine {
public:
    CCommandLine();              // 0x10147778
    ~CCommandLine();             // 0x101478d0
    int32_t GetArgc();           // 0x10147958
    char** GetArgv();            // 0x1014798c

    int32_t mArgc;               // 0x00
    char* mArgv[0x80];           // 0x04  pointers into mText
    char* mText;                 // 0x204  the command line's own copy (_strdup), cut up in place
};
