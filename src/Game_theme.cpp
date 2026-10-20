#include "Game.h"

#include "TPEngine.h"
#include "TPString.h"

int32_t CGame::GetCurrentTheme() const { return mCurrentTheme; }

const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() { return mCurrentThemeName; }


// The theme folder names (built by __sinit_Game_cpp on the Mac, with the same text; the order of the comparisons below is the
// order of these addresses going down: fantasy, hallow, jungle, space).
TbDynamicStringTemplate<char16_t> gGameThemeFantasy(u"fantasy");   // 0x1037f10c
TbDynamicStringTemplate<char16_t> gGameThemeHallow(u"hallow");     // 0x1037f0ec
TbDynamicStringTemplate<char16_t> gGameThemeJungle(u"jungle");     // 0x1037f0cc
TbDynamicStringTemplate<char16_t> gGameThemeSpace(u"space");       // 0x1037f0ac

// The two gate signs of a theme. The theme's gate ride type (fantasy 4601, hallow 2601, jungle 1601, space 3601) gives the sign
// texts through the ride database into two buffers of 32 characters, which are NOT cleared first: a theme that is none of the
// four assigns whatever the buffers hold. The assignments go through the strings' virtual operator=(const Char*). The local
// narrow string is constructed and destroyed and never used (kept so that the calls run as on the Mac).
void CGame::GetLocalisedThemeGateNames(const TbDynamicStringTemplate<char16_t>& theme, TbDynamicStringTemplate<char16_t>& gate1,
                                       TbDynamicStringTemplate<char16_t>& gate2) const {
    TbDynamicStringTemplate<char> unused;
    char16_t second[0x20];
    char16_t first[0x20];
    if (TPUniStrEq(theme, gGameThemeFantasy)) {
        const uint16_t type = 0x11f9;
        gRideDatabase.GetSignNameA(type, first, 0x20);
        gRideDatabase.GetSignNameB(type, second, 0x20);
    } else if (TPUniStrEq(theme, gGameThemeHallow)) {
        const uint16_t type = 0xa29;
        gRideDatabase.GetSignNameA(type, first, 0x20);
        gRideDatabase.GetSignNameB(type, second, 0x20);
    } else if (TPUniStrEq(theme, gGameThemeJungle)) {
        const uint16_t type = 0x641;
        gRideDatabase.GetSignNameA(type, first, 0x20);
        gRideDatabase.GetSignNameB(type, second, 0x20);
    } else if (TPUniStrEq(theme, gGameThemeSpace)) {
        const uint16_t type = 0xe11;
        gRideDatabase.GetSignNameA(type, first, 0x20);
        gRideDatabase.GetSignNameB(type, second, 0x20);
    }
    gate1 = static_cast<const char16_t*>(first);
    gate2 = static_cast<const char16_t*>(second);
}
