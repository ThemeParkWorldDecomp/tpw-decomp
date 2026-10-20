// SoundBalance.cpp (SimTheme Park 2.04b3).
// Static initializer 0x10021750; emitted members 0x100217d4, 0x10021820, 0x10021864.
#include "SoundBalance.h"

// Typed description rows at 0x1029b744 (sound.sam), ending with type 12. Value slots count
// the rows that are not headings or separators: QSOUND 1 ... SOUNDON 11, SFX 12, MUSIC 13,
// SPEECH 14, MOVIE 15.
const BalanceDescription gSoundBalanceDescriptions[28] = {
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "QSOUND", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "REAR", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "CROSSOVER", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "EAX", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "GEOMETRY", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "MPEGRATE", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "DUCKINGLEVEL", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "DROPPABLELEVEL", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "DEFAULTQUALITY", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "FADETOGGLE", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "SOUNDON", 0, 2, 0.0f, 0.0f, 0, 0u},
    {1, "SoundInfo", 0, 0, 0.0f, 0.0f, 0, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "SFX", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "MUSIC", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "SPEECH", 0, 101, 0.0f, 0.0f, 0, 0u},
    {6, "MOVIE", 0, 101, 0.0f, 0.0f, 0, 0u},
    {1, "DefaultVolume", 0, 0, 0.0f, 0.0f, 0, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "CHANNELS", 5, 14, 0.0f, 0.0f, 0, 0u},
    {6, "SWITCH", 0, 101, 0.0f, 0.0f, 0, 0u},
    {3, "ChannelInfo", 0, 0, 0.0f, 0.0f, 3, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {9, "MINRADIUS", 0, 0, 0.0f, 101.0f, 0, 0u},
    {6, "SWITCH", 0, 101, 0.0f, 0.0f, 0, 0u},
    {3, "RadiusInfo", 0, 0, 0.0f, 0.0f, 3, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

const BalanceDescription* CSoundBalance::GetBalanceDescription(int32_t row) {
    return &gSoundBalanceDescriptions[row];
}
BalanceValue* CSoundBalance::GetBalanceValuesStart() { return mValues; }
CSoundBalance::~CSoundBalance() { UnInit(); }

CSoundBalance gSoundBalance; // __sinit_SoundBalance_cpp at 0x10021750, object at 0x102b4e2c
