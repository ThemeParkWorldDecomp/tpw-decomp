// CGameType (main app 0x1016f5d8..0x1016f79c; the inventory files it under FS_Online.cpp; unit w40): the game's type (normal, online
// or "easy mode" master-save user) and the three bits of the game option word that mirror it. The one object is TheGameType()
// (include/FS_Online.h), made on first use.
#include "Debug.h"
#include "FS_Online.h"
#include "TPMain.h"

// 0x1016f5d8. The type starts as 0 and is then set from the option bits already in the word: easy mode wins over online, online over
// normal, and none of the three is the normal game. Every path goes through SetGameType, so the word ends with exactly one bit.
CGameType::CGameType() : mType(kGameTypeNormal) {
    const uint32_t options = gGameOptions;
    if ((options & kGameOptionEasyGame) != 0) {
        SetGameType(kGameTypeEasy);
    } else if ((options & kGameOptionOnlineGame) != 0) {
        SetGameType(kGameTypeOnline);
    } else if ((options & kGameOptionNormalGame) != 0) {
        SetGameType(kGameTypeNormal);
    } else {
        SetGameType(kGameTypeNormal);
    }
}

// 0x1016f6a0. The assertion tests `type < 3` as a signed number (-1 passes) and the type is stored before the switch whatever it is,
// so 3 and up, or negative, change the object but not the option word. Each of the three leaves its own bit set and the others clear.
void CGameType::SetGameType(GameType type) {
    GAME_ASSERTMSG(type < 3, "Invalid GameType in SetGameType\n");
    mType = type;
    switch (type) {
    case kGameTypeNormal:
        gGameOptions |= kGameOptionNormalGame;
        gGameOptions &= ~kGameOptionOnlineGame;
        gGameOptions &= ~kGameOptionEasyGame;
        break;
    case kGameTypeOnline:
        gGameOptions &= ~kGameOptionNormalGame;
        gGameOptions |= kGameOptionOnlineGame;
        gGameOptions &= ~kGameOptionEasyGame;
        break;
    case kGameTypeEasy:
        gGameOptions &= ~kGameOptionNormalGame;
        gGameOptions &= ~kGameOptionOnlineGame;
        gGameOptions |= kGameOptionEasyGame;
        break;
    default:
        break;
    }
}
