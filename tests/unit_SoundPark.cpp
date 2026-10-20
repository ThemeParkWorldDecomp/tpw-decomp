// Fakes and test entry points for unit w74, the park sound wrappers (libtpw_unit_SoundPark.dylib, driven by
// tests/test_SoundPark.py; see calllog.h):
//   src/SoundPark.cpp  SND_DipMusic 0x100deb04, SND_StartParkMusic 0x100deb40, SND_StopParkMusic 0x100debd4, SND_SetParkMusicLevel
//                      0x100dec30, SND_SetChatterLevel 0x100dec9c, SND_SetChatterOnOff 0x100deda4, SND_SetRainLevel 0x100dee84,
//                      SND_SendIndirectedEvent 0x100df858, SOUND_SaveStatus 0x100de1a0, SOUND_LoadStatus 0x100de48c
//   src/TPSound.cpp    the state (gSoundSettings, the flags, the banks)
// Fakes, each recording what the original's callee would be given (the PowerPC side records the same from its stubs): the runtime
// wrappers SOUND_SendEvent, SOUND_SetParamValue and SOUND_FadeSoundEffect (the queues "Event" and "Param" are their answers),
// RSSE_GetSoundEventCode (the queue "Code"), LbFile_Read/Write/Seek over one file the case sets up (a byte string and a position;
// what the writes add is kept for a read-back), the SamsUtilities swaps (nothing on a little-endian host) and the audio object (two
// of them, so that a command can make the game switch objects between its two commands).
//
// The case drives the functions with tpw_sx_op and reads the state back with tpw_sx_get after every operation. The state is one
// numbered list of words (the PowerPC side reads the same words from the Mac's addresses): see tpw_sx_get.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "LbFile.h"
#include "TPSound.h"
#include "TbAudioSystem.h"
#include "calllog.h"
#include "rsse.h"

using namespace calllog;

namespace {

constexpr int kLabelAudio = 2;
constexpr int kLabelAudio2 = 4;
constexpr int kLabelFile = 5;
constexpr uint32_t kSettingsSize = 0x54;

// The kinds of the commands, as in tests/test_SoundPark.py.
enum Kind { kSave = 1, kLoad = 2, kSetPresetReverb = 3, kUnknown = 99 };

// The file: its bytes, the position, what has been written since the last rewind, and what the audio object's Save adds.
std::vector<uint8_t> gFile;
std::size_t gPosition;
std::vector<uint8_t> gFileAtStart;   // what the case set up (ResetFile puts it back)
std::size_t gPositionAtStart;
std::vector<uint8_t> gWritten;
std::vector<uint8_t> gObjectBytes;
alignas(16) unsigned char gFileObject[16];
// The swaps: the address of the first swapped word outside the settings block since the last file call (the save's copy).
const void* gSwapBase;

void put_state(int32_t index, int32_t value);

// A poke from the queue `key`: (index + 1) << 32 | value changes one word of the state after the call (0: none).
void poke(const char* key) {
    const int64_t packed = ret(key, 0);
    if (packed == 0) return;
    put_state(static_cast<int32_t>((packed >> 32) - 1), static_cast<int32_t>(packed & 0xFFFFFFFF));
}

// Hex of bytes [from, to) of a buffer, "" when empty.
std::string Hex(const unsigned char* bytes, uint32_t from, uint32_t to) {
    static const char digits[] = "0123456789abcdef";
    std::string text;
    for (uint32_t i = from; i < to; ++i) {
        text += digits[bytes[i] >> 4];
        text += digits[bytes[i] & 15];
    }
    return text;
}

class FakeAudio : public TbAudioSystem {
public:
    TbError Release() override { return TbError(0); }
    TbError SetListenerOrientation(const TbSoundPosition&, const TbSoundPosition&, const TbSoundPosition&,
                                   const TbSoundPosition&) override {
        return TbError(0);
    }
    TbError RegisterCatagory(const char*, const char*, uint32_t&) override { return TbError(0); }
    int32_t SendEvent(int32_t, const TbAudioEvent&) override { return 0; }
    int32_t UpdateHandle(int32_t, const TbSoundPosition&, const TbSoundPosition&) override { return 0; }
    int32_t UpdateParameter(int32_t, uint8_t, uint32_t) override { return 0; }
    // The command: its kind and up to two payload words. Save adds the case's object bytes to the file's written bytes and
    // records how many; Load records the file and the flag byte; SetPresetReverb records the preset. The queue "PokeCmd" can
    // change one word of the state afterwards (the first command of a load can change the settings or the audio object).
    TbError SystemCommand(const TbSysCommand::Command& command) const override {
        int64_t kind = kUnknown;
        int64_t p[2] = {0, 0};
        if (auto* c = dynamic_cast<const TbSysCommand::Save*>(&command)) {
            kind = kSave;
            p[0] = lab(c->mFile);
            p[1] = static_cast<int64_t>(gObjectBytes.size());
            gWritten.insert(gWritten.end(), gObjectBytes.begin(), gObjectBytes.end());
        } else if (auto* c = dynamic_cast<const TbSysCommand::Load*>(&command)) {
            kind = kLoad;
            p[0] = lab(c->mFile);
            p[1] = c->mFlag;
        } else if (auto* c = dynamic_cast<const TbSysCommand::SetPresetReverb*>(&command)) {
            kind = kSetPresetReverb;
            p[0] = c->mValue;
        }
        ev("Audio.SystemCommand", {lab(this), kind, p[0], p[1]});
        poke("PokeCmd");
        return TbError(0);
    }
    TbError Init(const char*, const CAudioConfig&) override { return TbError(0); }
    TbError UpdateDistortion(void*, float) override { return TbError(0); }
    TbError UpdateLowPass(void*, float) override { return TbError(0); }
    TbError StopHandle(void*) const override { return TbError(0); }
    TbError DeregisterCatagoryBanks(uint32_t) const override { return TbError(0); }
    TbError CreateSystemChain(const CAudioConfig&) override { return TbError(0); }
};
FakeAudio gAudio;
FakeAudio gAudio2;

int32_t settings_entry(int32_t entry) {
    const unsigned char* base = reinterpret_cast<const unsigned char*>(&gSoundSettings);
    if (entry == 12) return gSoundSettings.mMusicMute;
    if (entry == 13) return gSoundSettings.mPauseOn3DOnly;
    int32_t word;
    std::memcpy(&word, base + (entry < 12 ? 4 * entry : 4 * (entry - 14) + 0x34), sizeof word);
    return word;
}

void put_state(int32_t index, int32_t value) {
    switch (index) {
    case 0: gSoundWanted = static_cast<uint32_t>(value); return;
    case 1: gSoundSystemOn = static_cast<uint32_t>(value); return;
    case 2: gAudioSystem = value == 1 ? &gAudio : value == 2 ? &gAudio2 : nullptr; return;
    case 25: gSoundBanks[0] = value; return;
    case 26: gSoundBanks[1] = value; return;
    case 27: gLocalParkMusicBank = value; return;
    default: break;
    }
    const int32_t entry = index - 3;
    unsigned char* base = reinterpret_cast<unsigned char*>(&gSoundSettings);
    if (entry == 12) {
        gSoundSettings.mMusicMute = static_cast<int16_t>(value);
    } else if (entry == 13) {
        gSoundSettings.mPauseOn3DOnly = static_cast<int16_t>(value);
    } else {
        std::memcpy(base + (entry < 12 ? 4 * entry : 4 * (entry - 14) + 0x34), &value, sizeof value);
    }
}

}  // namespace

// ---- the callees outside the unit
// The runtime wrappers: recorded; the answers come from the queues.
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return static_cast<int32_t>(ret("Event", 0));
}
int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value) {
    ev("SOUND_SetParamValue", {handle, param, value});
    return static_cast<int32_t>(ret("Param", 0));
}
void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }
// rsse.cpp's (untranslated): the queue "Code" is its answer.
int32_t RSSE_GetSoundEventCode(int32_t a, int32_t b) {
    ev("RSSE_GetSoundEventCode", {a, b});
    return static_cast<int32_t>(ret("Code", 0));
}

// The file: a read copies what is left of the file (at most the length; at most 0x54 into the settings block, which is where a
// longer size would run on into the words after it on the Mac), and the event says how many bytes that was; transferred is
// recorded as set or not (the game gives NULL). The queue "PokeRead" can change one word afterwards.
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    gSwapBase = nullptr;
    const bool inSettings = buffer == static_cast<void*>(&gSoundSettings);
    const bool inWanted = buffer == static_cast<void*>(&gSoundWanted);
    const std::size_t left = gFile.size() - gPosition;
    std::size_t count = length < left ? length : left;
    if (inSettings && count > kSettingsSize) count = kSettingsSize;
    std::memcpy(buffer, gFile.data() + gPosition, count);
    gPosition += count;
    if (transferred != nullptr) *transferred = static_cast<uint32_t>(count);
    ev("LbFile_Read", {lab(file), length, inSettings ? 1 : inWanted ? 2 : 0, static_cast<int64_t>(count), transferred != nullptr ? 1 : 0});
    poke("PokeRead");
    return TbError(0);
}
// A write is logged with its bytes (in three texts, as the log's texts are short) and added to the written bytes. The queue
// "PokeWrite" can change one word afterwards.
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    gSwapBase = nullptr;
    const unsigned char* bytes = static_cast<const unsigned char*>(buffer);
    const uint32_t third = (length + 2) / 3;
    const std::string a = Hex(bytes, 0, length < third ? length : third);
    const std::string b = Hex(bytes, third < length ? third : length, 2 * third < length ? 2 * third : length);
    const std::string c = Hex(bytes, 2 * third < length ? 2 * third : length, length);
    ev("LbFile_Write", {lab(file), length, transferred != nullptr ? 1 : 0}, {a.c_str(), b.c_str(), c.c_str()});
    gWritten.insert(gWritten.end(), bytes, bytes + length);
    poke("PokeWrite");
    return TbError(0);
}
int32_t LbFile_Seek(void* file, int32_t offset, uint32_t whence) {
    ev("LbFile_Seek", {lab(file), offset, whence});
    const int64_t position = static_cast<int64_t>(gPosition) + offset;
    gPosition = static_cast<std::size_t>(position < 0 ? 0 : position > static_cast<int64_t>(gFile.size()) ? static_cast<int64_t>(gFile.size()) : position);
    return 0;
}

// The main app's swaps put a file's little-endian word in the host's order: nothing on a little-endian host. Each call is recorded
// with the offset of its word: in the settings block, or from the first word swapped since the last file call (the save's copy).
namespace {
int64_t SwapOffset(const void* word) {
    const unsigned char* address = static_cast<const unsigned char*>(word);
    const unsigned char* settings = reinterpret_cast<const unsigned char*>(&gSoundSettings);
    if (address >= settings && address < settings + sizeof gSoundSettings) return address - settings;
    if (gSwapBase == nullptr) gSwapBase = word;
    return address - static_cast<const unsigned char*>(gSwapBase);
}
}  // namespace
namespace SamsUtilities {
void USwapInplace16(int16_t& word) { ev("USwapInplace16", {SwapOffset(&word)}); }
void USwapInplace32(int32_t& word) { ev("USwapInplace32", {SwapOffset(&word)}); }
}  // namespace SamsUtilities

extern "C" {
// The state, by number: 0 wanted, 1 on, 2 the audio object's label, 3 to 24 the settings block (entries 0 to 11: the words at
// 0x00 to 0x2c, 12 and 13: the shorts at 0x30 and 0x32, 14 to 21: the words at 0x34 to 0x50), 25 the file position. The
// bytes the settings hold unswapped (entry 3, 18 to 21) are words in the host's own order, which is what the PowerPC side
// reads from the bytes in little-endian order.
int32_t tpw_sx_get(int32_t index) {
    switch (index) {
    case 0: return static_cast<int32_t>(gSoundWanted);
    case 1: return static_cast<int32_t>(gSoundSystemOn);
    case 2: return lab(gAudioSystem);
    case 25: return static_cast<int32_t>(gPosition);
    default: return settings_entry(index - 3);
    }
}

// Puts one word of the state: 0 wanted, 1 on, 2 the audio object (0, 1 or 2), 3 to 24 the settings, 25 and 26 the banks 0
// and 1, 27 the park music's bank.
void tpw_sx_put(int32_t index, int32_t value) { put_state(index, value); }

// Starts a case: the state cleared, the file empty, nothing written.
void tpw_sx_begin() {
    std::memset(&gSoundSettings, 0, sizeof gSoundSettings);
    std::memset(gSoundBanks, 0, sizeof gSoundBanks);
    gLocalParkMusicBank = 0;
    gAudioSystem = nullptr;
    gSoundWanted = 0;
    gSoundSystemOn = 0;
    gFile.clear();
    gFileAtStart.clear();
    gPositionAtStart = 0;
    gWritten.clear();
    gObjectBytes.clear();
    gPosition = 0;
    gSwapBase = nullptr;
}

// The file's bytes, its position and the bytes the audio object's Save adds.
void tpw_sx_set_file(const uint8_t* bytes, int32_t count, int32_t position) {
    gFile.assign(bytes, bytes + count);
    gPosition = static_cast<std::size_t>(position);
    gFileAtStart = gFile;
    gPositionAtStart = gPosition;
}
void tpw_sx_set_object_bytes(const uint8_t* bytes, int32_t count) { gObjectBytes.assign(bytes, bytes + count); }

// The objects the labels stand for: 2 and 4 the audio objects, 5 the file.
void* tpw_sx_object(int32_t label) {
    if (label == kLabelAudio) return &gAudio;
    if (label == kLabelAudio2) return &gAudio2;
    if (label == kLabelFile) return gFileObject;
    return nullptr;
}

// What has been written since the last rewind, as hex (for the final comparison).
const char* tpw_sx_written() {
    static std::string text;
    text = Hex(gWritten.data(), 0, static_cast<uint32_t>(gWritten.size()));
    return text.c_str();
}

// One operation; `a` holds its arguments; the answer is its result. Operation numbers are in test_SoundPark.py.
int64_t tpw_sx_op(int32_t op, const int64_t* a) {
    const int32_t i0 = static_cast<int32_t>(a[0]);
    void* file = a[0] != 0 ? static_cast<void*>(gFileObject) : nullptr;
    switch (op) {
    case 0: SND_DipMusic(i0); return 0;
    case 1: SND_StartParkMusic(); return 0;
    case 2: SND_StopParkMusic(); return 0;
    case 3: SND_SetParkMusicLevel(i0); return 0;
    case 4: SND_SetChatterLevel(i0); return 0;
    case 5: SND_SetChatterOnOff(i0); return 0;
    case 6: SND_SetRainLevel(i0); return 0;
    case 7:
        return SND_SendIndirectedEvent(i0, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]),
                                       static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5]), static_cast<int32_t>(a[6]));
    case 8: SOUND_SaveStatus(file); return 0;
    case 9: SOUND_LoadStatus(file); return 0;
    case 10:
        // Rewind: the bytes written so far become the file (the position 0).
        gFile = gWritten;
        gWritten.clear();
        gPosition = 0;
        return 0;
    case 11:
        // ResetFile: the file and the position the case started with.
        gFile = gFileAtStart;
        gPosition = gPositionAtStart;
        return 0;
    }
    return -12345;
}
}
