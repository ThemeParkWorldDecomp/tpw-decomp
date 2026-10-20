// CActionRecorder::ReplayRecording (0x10144e2c) and EndReplay (0x10144ce4), unit v5224.
#include <cstdint>

#include "ActionRecorder.h"
#include "Debug.h"
#include "TPCoaster.h"
#include "ThingReceivers.h"
#include "World.h"
#include "bfl_FileSystem.h"
#include "platform/samsutils.h"
#include "rsse.h"

#include <cstring>
#include <string>

#include "FS_MasterSave.h"
#include "platform/samsutils.h"

namespace {
using CFile = bfl::FileSystem::CFile;
CFile* ReplayFileOf(uint8_t* slot) { return reinterpret_cast<CFile*>(slot); }

uint16_t Swap16(uint16_t v) { return static_cast<uint16_t>((v << 8) | (v >> 8)); }
uint32_t Swap32(uint32_t v) {
    return (v >> 24) | ((v >> 8) & 0xff00u) | ((v << 8) & 0xff0000u) | (v << 24);
}
// Writes the Mac's memory image of a big-endian value: most significant byte first.
void PutBE(CFile* file, uint32_t value, uint32_t size) {
    uint8_t bytes[4];
    for (uint32_t i = 0; i < size; i++) bytes[i] = static_cast<uint8_t>(value >> (8 * (size - 1 - i)));
    file->Write(bytes, size);
}
uint32_t FloatBits(float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}
float BitsFloat(uint32_t b) {
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}
}  // namespace

// 0x10144704
void CActionRecorder::RecordAction(RecordedAction action, float value, uint16_t id) {
    if (mRecording != 0) return;
    if (mState == 0) return;
    CFile* file = ReplayFileOf(mActionFile);
    uint8_t code = static_cast<uint8_t>(action);
    uint16_t swappedId = Swap16(id);
    uint32_t swappedValue = Swap32(FloatBits(value));
    PutBE(file, code, 1);
    PutBE(file, swappedValue, 4);
    PutBE(file, swappedId, 2);
    CTRACE(2, "AR - %d (%g, %d)\n", static_cast<int32_t>(action), static_cast<double>(BitsFloat(swappedValue)),
           static_cast<uint32_t>(swappedId));
}

// 0x10144868
void CActionRecorder::RecordAction(RecordedAction action, float a, float b, uint16_t id) {
    if (mRecording != 0) return;
    if (mState == 0) return;
    CFile* file = ReplayFileOf(mActionFile);
    uint8_t code = static_cast<uint8_t>(action);
    uint32_t swappedA = Swap32(FloatBits(a));
    uint32_t swappedB = Swap32(FloatBits(b));
    uint16_t swappedId = Swap16(id);
    PutBE(file, code, 1);
    PutBE(file, swappedA, 4);
    PutBE(file, swappedB, 4);
    PutBE(file, swappedId, 2);
    CTRACE(2, "AR - %d (%g, %g, %d)\n", static_cast<int32_t>(action), static_cast<double>(BitsFloat(swappedA)),
           static_cast<double>(BitsFloat(swappedB)), static_cast<uint32_t>(swappedId));
}

// 0x10144a00
void CActionRecorder::RecordAction(RecordedAction action, uint16_t a, uint8_t b, const char16_t* text) {
    if (mRecording != 0) return;
    if (mState == 0) return;
    std::u16string str(text);
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
    reinterpret_cast<SwearFilter*>(gMasterSave)->Filter(str, true);
    CFile* file = ReplayFileOf(mActionFile);
    uint8_t code = static_cast<uint8_t>(action);
    uint16_t swappedA = Swap16(a);
    // The original calls SamsUtilities::_USwapBlock16 on the text; the port's function does not swap on this little-endian host, so the
    // swap is done here to keep the Mac's observable bytes: each unit's halves are exchanged.
    for (char16_t& unit : str) unit = static_cast<char16_t>(Swap16(static_cast<uint16_t>(unit)));
    PutBE(file, code, 1);
    PutBE(file, swappedA, 2);
    PutBE(file, b, 1);
    // The Mac's memory image of the swapped units: big-endian.
    std::string image;
    for (char16_t unit : str) {
        image.push_back(static_cast<char>(unit >> 8));
        image.push_back(static_cast<char>(unit & 0xff));
    }
    file->Write(image.data(), static_cast<uint32_t>(image.size()));
    CTRACE(2, "AR - %d (%d, %d, %ls)\n", static_cast<int32_t>(action), static_cast<uint32_t>(swappedA), static_cast<uint32_t>(b),
           str.c_str());
}

void CActionRecorder::ReplayRecording(ReplayType type) {
    if (mRecording != 0) return;
    mReplayType = type;
    mRecording = 1;
    gGameWorld->GetBankAccount()->mWithdrawalsEnabled = 0;
    if (mMode == 0 && (mReplayType == 2 || mReplayType == 1)) {
        ReplayFileOf(mTimingFile)->Seek(0, 0);
        GetNextTimingFileEntry();
        ReplayFileOf(mCameraFile)->Seek(0, 0);
        GetNextCameraFileEntry();
    }
    ReplayFileOf(mActionFile)->Seek(0, 0);
    CTRACE(0x499602d2, "[[[[ Starting layout replay ]]]]\n");
    DoTick();
}

void CActionRecorder::EndReplay() {
    ReplayFileOf(mActionFile)->Seek(0, 2);
    if (mMode == 0) {
        ReplayFileOf(mTimingFile)->Seek(0, 2);
        ReplayFileOf(mCameraFile)->Seek(0, 2);
    }
    mRecording = 0;
    mF08 = 1;
    RSSE_Tick();
    RSSE_Tick();
    RSSE_Tick();
    RSSE_Tick();
    RSSE_Tick();
    RSSE_Tick();
    RSSE_Tick();
    RSSE_Tick();
    LayoutStartAllCoasterSounds();
    gGameWorld->GetBankAccount()->mWithdrawalsEnabled = 1;
    CTRACE(0x499602d2, "[[[[ Finished layout replay ]]]]\n");
}
