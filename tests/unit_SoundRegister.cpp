// Fakes and test entry points for unit w65, the sound registration layer (libtpw_unit_SoundRegister.dylib, driven by
// tests/test_SoundRegister.py; see calllog.h):
//   src/game/Sound.cpp, src/game/park/ParkSound.cpp  SOUND_Malloc 0x100dc5e4, SOUND_Free 0x100dc64c, SOUND_RegisterCatagory (both forms) 0x100dc6ac and
//                          0x100dc74c, SOUND_FreeCatagory 0x100dc89c, SND_RegisterGlobalSounds 0x100df310 and the other
//                          SND_Register*/SND_Free* functions (0x100def68 to 0x100df778)
//   src/game/Sound.cpp, src/game/park/ParkSound.cpp        the state they work on (the flags, the audio object, the banks, the island table)
// Real: those and the bullfrog strings. Fakes, each recording what the original's callee would be given (the PowerPC side
// records the same from its stubs): the audio object (a TbAudioSystem whose RegisterCatagory answers the handles of the case's
// queue "Handle" and whose SystemCommand records the command's kind and category), the resource manager's FindResource (the
// queue "Find" says what it finds), CTPSoundResource::FindSoundFile (the queue "Sound" is its answer; a nonzero answer also
// writes the path "P_<name>"), GAME_ASSERTMSG, and the library heap LbMem_Alloc and LbMem_Free (blocks of the library are
// zeroed and labelled; the flag set by tpw_sr_alloc_fail makes the next allocation fail).
//
// The case drives the functions with tpw_sr_op and reads the state back with tpw_sr_get after every operation. The state is
// one numbered list of words (the PowerPC side reads the same words from the Mac's addresses): see kState below.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "Game.h"
#include "LbFile.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPSound.h"
#include "Advisor.h"
#include "TbAudioSystem.h"
#include "calllog.h"
#include "lobby.h"

using namespace calllog;
using String = TbDynamicStringTemplate<char>;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

// ---- the game object and the globals (raw storage: their constructors are not wanted here)
#ifdef __APPLE__
extern unsigned char gGameStorage[sizeof(CGame)] __asm__("_gGame");
extern unsigned char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
#else
extern unsigned char gGameStorage[sizeof(CGame)] __asm__("gGame");
extern unsigned char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("gTPGlobals");
#endif
alignas(16) unsigned char gGameStorage[sizeof(CGame)];
alignas(16) unsigned char gTPGlobalsStorage[sizeof(TPGlobals)];

// The string constructors the real strings need and nothing here calls.
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

namespace {
// What the fakes stand for. Labels: 1 the resource manager, 2 the audio object, 3 to 6 the four sound resources; the stranger is a
// block the library never handed out (no label).
constexpr int kLabelManager = 1;
constexpr int kLabelAudio = 2;
constexpr int kLabelResource = 3;
constexpr int kLabelStranger = 7;
alignas(16) unsigned char gManagerStorage[16];
alignas(16) unsigned char gResources[4][64];
alignas(16) unsigned char gStranger[16];
void* gBlocks[16];
bool gAllocFail;

class FakeAudio : public TbAudioSystem {
public:
    TbError Release() override {
        ev("Audio.Release", {lab(this)});
        return TbError(0);
    }
    TbError SetListenerOrientation(const TbSoundPosition&, const TbSoundPosition&, const TbSoundPosition&,
                                   const TbSoundPosition&) override {
        ev("Audio.SetListenerOrientation", {lab(this)});
        return TbError(0);
    }
    TbError RegisterCatagory(const char* path, const char* name, uint32_t& handle) override {
        ev("Audio.RegisterCatagory", {lab(this)}, {path, name});
        handle = static_cast<uint32_t>(ret("Handle", 0));
        return TbError(0);
    }
    int32_t SendEvent(int32_t, const TbAudioEvent&) override {
        ev("Audio.SendEvent", {lab(this)});
        return 0;
    }
    int32_t UpdateHandle(int32_t, const TbSoundPosition&, const TbSoundPosition&) override {
        ev("Audio.UpdateHandle", {lab(this)});
        return 0;
    }
    int32_t UpdateParameter(int32_t, uint8_t, uint32_t) override {
        ev("Audio.UpdateParameter", {lab(this)});
        return 0;
    }
    // The command's kind (1 for a flush) and its category.
    TbError SystemCommand(const TbSysCommand::Command& command) const override {
        const auto* flush = dynamic_cast<const TbSysCommand::FlushCatagory*>(&command);
        ev("Audio.SystemCommand", {lab(this), flush != nullptr ? 1 : 0, flush != nullptr ? flush->mCategory : 0});
        return TbError(0);
    }
    TbError Init(const char*, const CAudioConfig&) override {
        ev("Audio.Init", {lab(this)});
        return TbError(0);
    }
    TbError UpdateDistortion(void*, float) override {
        ev("Audio.UpdateDistortion", {lab(this)});
        return TbError(0);
    }
    TbError UpdateLowPass(void*, float) override {
        ev("Audio.UpdateLowPass", {lab(this)});
        return TbError(0);
    }
    TbError StopHandle(void*) const override {
        ev("Audio.StopHandle", {lab(this)});
        return TbError(0);
    }
    TbError DeregisterCatagoryBanks(uint32_t) const override {
        ev("Audio.DeregisterCatagoryBanks", {lab(this)});
        return TbError(0);
    }
    TbError CreateSystemChain(const CAudioConfig&) override {
        ev("Audio.CreateSystemChain", {lab(this)});
        return TbError(0);
    }
};
FakeAudio gAudio;

CGame& game() { return *reinterpret_cast<CGame*>(gGameStorage); }
TPGlobals& globals() { return *reinterpret_cast<TPGlobals*>(gTPGlobalsStorage); }
}  // namespace

// ---- the callees outside the unit
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// The library heap: zeroed, labelled blocks (a freed block loses its label).
void* LbMem_Alloc(uint32_t size) {
    if (gAllocFail) {
        ev("LbMem_Alloc", {size, 0});
        return nullptr;
    }
    void* block = std::calloc(size == 0 ? 1 : size, 1);
    ev("LbMem_Alloc", {size, 1});
    newlab(block);
    return block;
}

void LbMem_Free(void* block) {
    ev("LbMem_Free", {lab(block)});
    if (lab(block) > 0) {
        forget(block);
        std::free(block);
    }
}

// What the resource manager finds: the queue "Find" gives a code per lookup. 0: nothing (answer 0). 1 to 4: sound resource
// number code (answer 1). 5: resource 1, but the answer is 2. 6: nothing, the answer is -1.
int32_t CTPResourceManager::FindResource(const String& name, CTPBaseResource** resource) {
    ev("FindResource", {lab(this)}, {name.mData});
    int64_t code = ret("Find", 0);
    if (code >= 1 && code <= 4) {
        *resource = reinterpret_cast<CTPBaseResource*>(gResources[code - 1]);
        return 1;
    }
    if (code == 5) {
        *resource = reinterpret_cast<CTPBaseResource*>(gResources[0]);
        return 2;
    }
    return code == 6 ? -1 : 0;
}

// The sound file search: the queue "Sound" is the answer (the original answers a char); a nonzero answer writes "P_<name>".
int32_t CTPSoundResource::FindSoundFile(const String& name, String& path) const {
    ev("FindSoundFile", {lab(this)}, {name.mData});
    int64_t answer = ret("Sound", 1);
    if (answer != 0) path = String((std::string("P_") + name.mData).c_str());
    return static_cast<int32_t>(answer);
}

// src/game/advisor/Advisor.cpp's (the unit does not link it).
int32_t gAdvisorThemeSoundBank;

extern "C" {
// The state, by number: the same words as the Mac's block (BSS 0x102f7c30 to 0x102f7c3c and 0x102f7c94 to 0x102f7cc4).
// 0 allocations, 1 audio object (0 or 1), 2 wanted, 3 on, 4 to 11 the banks, 12 gAdvisorThemeSoundBank, 13 the music bank, 14 the
// thunder bank, 15 the island table (its label), 16 the island count.
int32_t tpw_sr_get(int32_t index) {
    switch (index) {
    case 0: return gSoundAllocations;
    case 1: return gAudioSystem != nullptr ? 1 : 0;
    case 2: return static_cast<int32_t>(gSoundWanted);
    case 3: return static_cast<int32_t>(gSoundSystemOn);
    case 12: return gAdvisorThemeSoundBank;
    case 13: return gLocalParkMusicBank;
    case 14: return gLobbyThunderBank;
    case 15: return lab(gLobbyIslandSounds);
    case 16: return gLobbyIslandSoundCount;
    default: return gSoundBanks[index - 4];
    }
}

void tpw_sr_put(int32_t index, int32_t value) {
    switch (index) {
    case 0: gSoundAllocations = value; break;
    case 1: gAudioSystem = value != 0 ? &gAudio : nullptr; break;
    case 2: gSoundWanted = static_cast<uint32_t>(value); break;
    case 3: gSoundSystemOn = static_cast<uint32_t>(value); break;
    case 12: gAdvisorThemeSoundBank = value; break;
    case 13: gLocalParkMusicBank = value; break;
    case 14: gLobbyThunderBank = value; break;
    default: gSoundBanks[index - 4] = value; break;
    }
}

// Word `index` of the island table (0 when there is none).
int32_t tpw_sr_table(int32_t index) { return gLobbyIslandSounds != nullptr ? gLobbyIslandSounds[index] : 0; }

// The objects the labels stand for: 1 the resource manager, 2 the audio object, 3 to 6 the resources, 7 the stranger.
void* tpw_sr_object(int32_t label) {
    if (label == kLabelManager) return gManagerStorage;
    if (label == kLabelAudio) return &gAudio;
    if (label == kLabelStranger) return gStranger;
    return gResources[label - kLabelResource];
}

// Starts a case: the game's resource manager, the five resource names, no blocks, the library heap working.
void tpw_sr_begin(const char* const* names) {
    game().mResourceManager = reinterpret_cast<CTPResourceManager*>(gManagerStorage);
    String* strings[5] = {&globals().mF4768, &globals().mF477C, &globals().mF4790, &globals().mF47A4, &globals().mF47B8};
    for (int i = 0; i < 5; ++i) new (strings[i]) String(names[i]);
    for (void*& block : gBlocks) block = nullptr;
    gAllocFail = false;
    gSoundAllocations = 0;
    gAudioSystem = nullptr;
    gSoundWanted = 0;
    gSoundSystemOn = 0;
    std::memset(gSoundBanks, 0, sizeof gSoundBanks);
    gAdvisorThemeSoundBank = 0;
    gLocalParkMusicBank = 0;
    gLobbyThunderBank = 0;
    gLobbyIslandSounds = nullptr;
    gLobbyIslandSoundCount = 0;
}

// Ends a case: frees what is left of the island table and of the blocks the case made.
void tpw_sr_end() {
    std::free(gLobbyIslandSounds);
    gLobbyIslandSounds = nullptr;
    for (void*& block : gBlocks) {
        std::free(block);
        block = nullptr;
    }
}

// One operation; the answer is its result (a label for a pointer). Operation numbers are in test_SoundRegister.py.
int64_t tpw_sr_op(int32_t op, int32_t a, int32_t b, const char* s1, const char* s2) {
    switch (op) {
    case 0:   // SOUND_Malloc(a) into block slot b
        gAllocFail = false;
        gBlocks[b] = SOUND_Malloc(a);
        return lab(gBlocks[b]);
    case 1:   // SOUND_Malloc(a) into block slot b, with the library heap failing
        gAllocFail = true;
        gBlocks[b] = SOUND_Malloc(a);
        gAllocFail = false;
        return lab(gBlocks[b]);
    case 2:   // SOUND_Free of block slot a (NULL when a < 0; the stranger when a is 99)
        SOUND_Free(a == 99 ? static_cast<void*>(gStranger) : a < 0 ? nullptr : gBlocks[a]);
        if (a >= 0 && a != 99) gBlocks[a] = nullptr;
        return 0;
    case 3: return SOUND_RegisterCatagory(s1, s2);
    case 4: return SOUND_RegisterCatagory(s1, *reinterpret_cast<const CTPSoundResource*>(gResources[a]));
    case 5: SOUND_FreeCatagory(a); return 0;
    case 6: SND_RegisterGlobalSounds(); return 0;
    case 7: SND_RegisterGlobalLobbySounds(); return 0;
    case 8: return SND_RegisterLocalLobbySounds(const_cast<char*>(s1));
    case 9: SND_FreeGlobalLobbySounds(); return 0;
    case 10: SND_FreeLocalLobbySounds(); return 0;
    case 11: SND_RegisterLocalParkSounds(String(s1)); return 0;
    case 12: SND_FreeGlobalSounds(); return 0;
    case 13: SND_FreeLocalParkSounds(); return 0;
    }
    return -12345;
}
}
