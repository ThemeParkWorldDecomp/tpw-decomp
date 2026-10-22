// ActionRecorder.cpp: name proven by __sinit_ActionRecorder_cpp in the main application.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "analyser.h"
#include "bfl_FileSystem.h"
#include "Clock.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_MasterSave.h"
#include "Game.h"
#include "InGame.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "MapCell.h"
#include "ParkTime.h"
#include "platform/samsutils.h"
#include "Ptr32.h"
#include "rsse.h"
#include "Serialise.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
#include "TbTList.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPSound.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include "WorldSavvy.h"
#include "WorldThings.h"
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

// ActionRecorder_CActionRecorder_CActionRecorder.cpp (main app): CActionRecorder constructor 0x10143658, destructor 0x10143880,
// Serialise 0x101439dc and RecordAction 0x1014430c (unit v5075).

namespace {

using bfl::FileSystem::CFile;
using bfl::FileSystem::CMemFile;

static_assert(sizeof(CMemFile) == 16, "a file slot of the recorder");

CFile* FileOf(uint8_t* slot) { return reinterpret_cast<CFile*>(slot); }

}  // namespace

// The table words are the Mac's offsets 0x34..0x3d3.
#define AR_WORD(offset) mActionKind[((offset) - 0x34) / 4]

void CActionRecorder::Construct() {
    new (mActionFile) CMemFile();    // 0x0c
    new (mTimingFile) CMemFile();    // 0x14
    new (mCameraFile) CMemFile();    // 0x1c
    mRecording = 0;
    CTRACE(2, "Start Action Recording\n");
    AR_WORD(0x34) = 1;
    AR_WORD(0x98) = 1;
    AR_WORD(0x48) = 3;
    AR_WORD(0xac) = 1;
    AR_WORD(0xc0) = 1;
    AR_WORD(0x5c) = 1;
    AR_WORD(0x70) = 0;
    AR_WORD(0x84) = 0;
    AR_WORD(0xd4) = 2;
    AR_WORD(0xe8) = 2;
    AR_WORD(0xfc) = 1;
    AR_WORD(0x110) = 8;
    AR_WORD(0x114) = 8;
    AR_WORD(0x118) = 4;
    AR_WORD(0x11c) = 4;
    AR_WORD(0x120) = 5;
    AR_WORD(0x124) = 7;
    AR_WORD(0x128) = 1;
    AR_WORD(0x12c) = 1;
    AR_WORD(0x130) = 6;
    AR_WORD(0x134) = 8;
    AR_WORD(0x138) = 8;
    AR_WORD(0x13c) = 1;
    AR_WORD(0x140) = 1;
    AR_WORD(0x144) = 1;
    AR_WORD(0x148) = 6;
    AR_WORD(0x14c) = 9;
    AR_WORD(0x160) = 9;
    AR_WORD(0x174) = 10;
    AR_WORD(0x188) = 10;
    AR_WORD(0x3d0) = 0xb;
    ActivateActionRecording(static_cast<RecordMode>(0));
    mF08 = 0;
}

void CActionRecorder::Destruct() {
    CTRACE(2, "End Action Recording\n");
    if (mMode == 0) {
        SaveRecordingToDisk();
        FileOf(mActionFile)->Close();
        FileOf(mTimingFile)->Close();
        FileOf(mCameraFile)->Close();
    } else {
        FileOf(mActionFile)->Close();
    }
    // The sub-objects go in reverse order of construction, each one a plain (non-virtual) destructor call.
    reinterpret_cast<CMemFile*>(mCameraFile)->CMemFile::~CMemFile();
    reinterpret_cast<CMemFile*>(mTimingFile)->CMemFile::~CMemFile();
    reinterpret_cast<CMemFile*>(mActionFile)->CMemFile::~CMemFile();
}

int32_t CActionRecorder::Serialise(void* file, int32_t saving, int32_t version, uint32_t) {
    CFile* memory = FileOf(mActionFile);
    if (saving != 0) {
        if (version < 2 && version >= 0) {
            if (SAVEVAR<int32_t>(file, &mF08, "mLoadedPublishedPark", 0) != 0) return 1;
            uint32_t size = static_cast<uint32_t>(memory->GetSize());
            if (SAVEVAR<uint32_t>(file, &size, "recording_size", 0) != 0) return 1;
            uint8_t* buffer = new uint8_t[size];
            uint32_t written = 0;
            memory->Seek(0, 0);
            memory->Read(buffer, size);
            memory->Seek(0, 2);
            TbError result = LbFile_Write(file, buffer, size, &written);
            if (static_cast<int32_t>(result) < 0) {
                delete[] buffer;
                return 1;
            }
            delete[] buffer;
        }
    } else {
        if (version < 2 && version >= 0) {
            ResetActionRecording();
            if (LOADVAR<int32_t>(file, &mF08, "mLoadedPublishedPark", 0) != 0) return 1;
            uint32_t size = 0;
            if (LOADVAR<uint32_t>(file, &size, "recording_size", 0) != 0) return 1;
            uint8_t* buffer = new uint8_t[size];
            uint32_t got = 0;
            TbError result = LbFile_Read(file, buffer, size, &got);
            if (static_cast<int32_t>(result) < 0) {
                delete[] buffer;
                return 1;
            }
            memory->Write(buffer, size);
            memory->Seek(0, 2);
            delete[] buffer;
        }
    }
    return 0;
}

// Unit v51072: CActionRecorder::ProcessAction (count, text), the 8 byte stand-ins of the recorder and the visitor panel, the destructors
// of the wide string, CMemFile and the thing id list, the file initialisers of InGame.cpp and ExitErr.cpp and a few key table actions.

namespace v51072_callee {
__attribute__((weak)) void OperatorDelete(void* block) { ::operator delete(block); }
}  // namespace v51072_callee

void CActionRecorder::SaveRecordingToDisk() {}

// Unit v51041: ActivateActionRecording and the RecordAction / ProcessAction overloads taking a RecordedAction.

namespace {

inline bfl::FileSystem::CFile* ActionFile(uint8_t* storage) { return reinterpret_cast<bfl::FileSystem::CFile*>(storage); }

inline bfl::FileSystem::CMemFile* MemFile(uint8_t* storage) { return reinterpret_cast<bfl::FileSystem::CMemFile*>(storage); }

}  // namespace

// 0x10143cf0 (184 bytes): starts recording in the given mode: opens the action file (and, in mode 0, the timing and camera files),
// then records the action 0xe7 with a zeroed 128-byte buffer.
void CActionRecorder::ActivateActionRecording(RecordMode mode) {
    uint8_t buffer[128];
    mMode = static_cast<int32_t>(mode);
    mState = 1;
    static_cast<void>(MemFile(mActionFile)->Open(0x200));
    if (static_cast<int32_t>(mode) == 0) {
        static_cast<void>(MemFile(mTimingFile)->Open(0x200));
        static_cast<void>(MemFile(mCameraFile)->Open(0x200));
    }
    for (int32_t i = 0; i < 0x80; ++i) buffer[i] = 0;
    RecordAction(static_cast<RecordedAction>(0xe7), buffer);
}

// ActionRecorder.cpp of the main app (SimTheme Park 2.04b3), unit w133: CActionRecorder::ResetActionRecording (0x10143e08, 260 bytes) and
// CActionRecorder::DoTick (0x10144f80, 872 bytes).

using bfl::FileSystem::CMemFile;

namespace {

// The recorder's three files. On the Mac they are CMemFile objects inside the recorder, made at start-up; here they are storage that is
// made into a CMemFile on first use (a zeroed object has no vtable word).
CMemFile& FileOf_2(uint8_t* storage) {
    static_assert(sizeof(CMemFile) <= 16, "the recorder's file storage");
    if (*reinterpret_cast<void**>(storage) == nullptr) new (storage) CMemFile();
    return *reinterpret_cast<CMemFile*>(storage);
}

// The action dictionary DoTick prints on every call while a replay runs (the original's text, one CTRACE each).
const char* const kDictionary[32] = {
    "Action recorder dictionary:",
    "---------------------------",
    "ACTION_SET_MODE=0",
    "ACTION_SET_RIDE=5",
    "ACTION_SET_RIDE_ROTATION=10",
    "ACTION_LMB_DOWN=15",
    "ACTION_LMB_UP=20",
    "ACTION_SET_MODE_NR=25",
    "ACTION_SET_RIDE_TRACK=30",
    "ACTION_SET_RIDE_QUEUE=35",
    "ACTION_LMB_DOWN_MODIFIED=40",
    "ACTION_LMB_UP_MODIFIED=45",
    "ACTION_SET_SELECTED_OBJ=50",
    "ACTION_COASTER_PLACENORMAL=55",
    "ACTION_COASTER_PLACESPECIAL=56",
    "ACTION_COASTER_LOFT=57",
    "ACTION_COASTER_ROTATE=58",
    "ACTION_COASTER_WOBBLE=59",
    "ACTION_COASTER_MOVE=60",
    "ACTION_COASTER_STACKUP=61",
    "ACTION_COASTER_STACKDOWN=62",
    "ACTION_COASTER_DELETEMULTI=63",
    "ACTION_COASTER_GENERATETRACKOLD=64",
    "ACTION_COASTER_BACKTRACK=65",
    "ACTION_COASTER_SETEDITOLD=66",
    "ACTION_COASTER_GENERATETRACK=67",
    "ACTION_COASTER_SETEDIT=68",
    "ACTION_SET_RIDE_NAME_A=70",
    "ACTION_SET_RIDE_NAME_B=75",
    "ACTION_SET_RIDE_NAME_A_WITH_MAPID=80",
    "ACTION_SET_RIDE_NAME_B_WITH_MAPID=85",
    "ACTION_NULL_DUMMY_PLACEHOLDER_USELESS_ACTION=999",
};

}  // namespace

// Starts a recording over: the state word is cleared, the action file is closed (and, in mode 0, the timing and camera files), the state
// becomes 1 and the same files are opened again (0x200 bytes each; the answers are dropped), then the action 0xe7 is recorded with a
// zeroed 128-byte buffer.
void CActionRecorder::ResetActionRecording() {
    uint8_t buffer[128];
    mState = 0;
    FileOf_2(mActionFile).Close();
    if (mMode == 0) {
        FileOf_2(mTimingFile).Close();
        FileOf_2(mCameraFile).Close();
    }
    const int32_t mode = mMode;
    mState = 1;
    static_cast<void>(FileOf_2(mActionFile).Open(0x200));
    if (mode == 0) {
        static_cast<void>(FileOf_2(mTimingFile).Open(0x200));
        static_cast<void>(FileOf_2(mCameraFile).Open(0x200));
    }
    for (int32_t i = 0; i < 0x80; ++i) buffer[i] = 0;
    RecordAction(0xe7, buffer);
}

// 0x10143f48 (108 bytes): the original writes the first byte of the action word as the Mac stores it (big-endian), the high byte.
void CActionRecorder::RecordAction(RecordedAction action) {
    if (mRecording != 0) return;
    if (mState == 0) return;
    uint8_t code = static_cast<uint8_t>(static_cast<uint32_t>(action) >> 24);
    ActionFile(mActionFile)->Write(&code, 1);
    CTRACE(2, "AR - %d\n", static_cast<int32_t>(action));
}

// 0x10143ff8 (160 bytes)
void CActionRecorder::RecordAction(RecordedAction action, uint8_t a, uint8_t b) {
    if (mRecording != 0) return;
    if (mState == 0) return;
    uint8_t bytes[3] = {static_cast<uint8_t>(action), a, b};
    ActionFile(mActionFile)->Write(bytes, 3);
    CTRACE(2, "AR - %d (%d, %d)\n", static_cast<int32_t>(action), static_cast<uint32_t>(a), static_cast<uint32_t>(b));
}

// 0x101440e0 (208 bytes)
void CActionRecorder::RecordAction(RecordedAction action, uint8_t a, uint8_t b, uint16_t c) {
    if (mRecording != 0) return;
    if (mState == 0) return;
    uint8_t bytes[3] = {static_cast<uint8_t>(action), a, b};
    uint16_t swapped = static_cast<uint16_t>((c << 8) | (c >> 8));
    // The Mac stores the exchanged halfword big-endian.
    uint8_t swappedBytes[2] = {static_cast<uint8_t>(swapped >> 8), static_cast<uint8_t>(swapped)};
    ActionFile(mActionFile)->Write(bytes, 3);
    ActionFile(mActionFile)->Write(swappedBytes, 2);
    CTRACE(2, "AR - %d (%d, %d, %d)\n", static_cast<int32_t>(action), static_cast<uint32_t>(a), static_cast<uint32_t>(b),
           static_cast<uint32_t>(swapped));
}

// 0x101441fc (200 bytes)
void CActionRecorder::RecordAction(RecordedAction action, uint16_t value) {
    if (mRecording != 0) return;
    if (mState == 0) return;
    uint8_t code = static_cast<uint8_t>(action);
    uint16_t swapped = static_cast<uint16_t>((value << 8) | (value >> 8));
    uint8_t swappedBytes[2] = {static_cast<uint8_t>(swapped >> 8), static_cast<uint8_t>(swapped)};
    ActionFile(mActionFile)->Write(&code, 1);
    ActionFile(mActionFile)->Write(swappedBytes, 2);
    CTRACE(2, "AR - %d (%d)\n", static_cast<int32_t>(action), static_cast<uint32_t>(swapped));
}

// 0x101445d0 (236 bytes)
void CActionRecorder::RecordAction(RecordedAction action, uint16_t value, uint8_t extra) {
    if (mRecording != 0) return;
    if (mState == 0) return;
    uint8_t code = static_cast<uint8_t>(action);
    uint16_t swapped = static_cast<uint16_t>((value << 8) | (value >> 8));
    uint8_t swappedBytes[2] = {static_cast<uint8_t>(swapped >> 8), static_cast<uint8_t>(swapped)};
    ActionFile(mActionFile)->Write(&code, 1);
    ActionFile(mActionFile)->Write(swappedBytes, 2);
    ActionFile(mActionFile)->Write(&extra, 1);
    CTRACE(2, "AR - %d (%d, %d)\n", static_cast<int32_t>(action), static_cast<uint32_t>(swapped), static_cast<uint32_t>(extra));
}

// 0x10144bec (176 bytes)
void CActionRecorder::RecordAction(RecordedAction action, uint8_t* data) {
    if (mRecording != 0) return;
    if (mState == 0) return;
    uint8_t code = static_cast<uint8_t>(action);
    ActionFile(mActionFile)->Write(&code, 1);
    ActionFile(mActionFile)->Write(data, 0x80);
    CTRACE(2, "AR - %d (128 byte array)\n", static_cast<int32_t>(action));
}

void CActionRecorder::RecordAction(RecordedAction action, uint16_t a, uint16_t b) {
    if (mRecording != 0) return;
    if (mState == 0) return;
    CFile* file = FileOf(mActionFile);
    uint8_t code = static_cast<uint8_t>(action);
    // The halves are exchanged and stored in the Mac's (big-endian) memory order.
    uint16_t swappedA = static_cast<uint16_t>((a << 8) | (a >> 8));
    uint16_t swappedB = static_cast<uint16_t>((b << 8) | (b >> 8));
    uint8_t bytesA[2] = {static_cast<uint8_t>(swappedA >> 8), static_cast<uint8_t>(swappedA)};
    uint8_t bytesB[2] = {static_cast<uint8_t>(swappedB >> 8), static_cast<uint8_t>(swappedB)};
    file->Write(&code, 1);
    file->Write(bytesA, 2);
    file->Write(bytesB, 2);
    CTRACE(2, "AR - %d (%d, %d)\n", static_cast<int32_t>(action), static_cast<uint32_t>(swappedA), static_cast<uint32_t>(swappedB));
}

// ActionRecorder.cpp (main app), unit v5362: ProcessAction 0x10145e38 (one halfword), ProcessAction 0x101465a8 (halfword, count, text) and
// RecordAction 0x1014445c (three halfwords).

// Names invented; initialised data of the main app, both 0.
uint16_t gReplayChainEndA = 0;   // 0x102a4bd4
uint16_t gReplayChainEndB = 0;   // 0x102a4cec

// 0x1014445c (296 bytes): records the action byte and three halfwords (each with its bytes exchanged) in the action file.
void CActionRecorder::RecordAction(int32_t action, uint16_t a, uint16_t b, uint16_t c) {
    if (mRecording != 0) return;
    if (mState == 0) return;
    bfl::FileSystem::CFile* file = reinterpret_cast<bfl::FileSystem::CFile*>(mActionFile);
    uint8_t code = static_cast<uint8_t>(action);
    uint16_t swappedA = static_cast<uint16_t>((a << 8) | (a >> 8));
    uint16_t swappedB = static_cast<uint16_t>((b << 8) | (b >> 8));
    uint16_t swappedC = static_cast<uint16_t>((c << 8) | (c >> 8));
    // The Mac stores the exchanged halfword big-endian.
    uint8_t bytesA[2] = {static_cast<uint8_t>(swappedA >> 8), static_cast<uint8_t>(swappedA)};
    uint8_t bytesB[2] = {static_cast<uint8_t>(swappedB >> 8), static_cast<uint8_t>(swappedB)};
    uint8_t bytesC[2] = {static_cast<uint8_t>(swappedC >> 8), static_cast<uint8_t>(swappedC)};
    file->Write(&code, 1);
    file->Write(bytesA, 2);
    file->Write(bytesB, 2);
    file->Write(bytesC, 2);
    CTRACE(2, "AR - %d (%d, %d, %d)\n", action, static_cast<uint32_t>(swappedA), static_cast<uint32_t>(swappedB),
           static_cast<uint32_t>(swappedC));
}

// CActionRecorder::ReplayRecording (0x10144e2c) and EndReplay (0x10144ce4), unit v5224.

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

// Does nothing unless a replay runs (mRecording). Then: the next camera file entry when the replay type is not 0, a trace of the turns to
// the next action, the dictionary (32 traces), and LLCM_SetIllegalPlacement(0). Type 0 plays every recorded action at once, then ends the
// replay. Types 1 and 2 (others do nothing) play the actions that are due: while no turns are left to the next one, trace the game turn
// (read once, before the loop), read and process the next action (end the replay and leave the loop when there is none) and fetch the next
// timing entry; then one turn is used up and, for type 1 after any action played, the wait is capped at 2 (an unsigned compare).
void CActionRecorder::DoTick() {
    if (mRecording == 0) return;
    if (mReplayType != 0) GetNextCameraFileEntry();
    CTRACE(0x499602d2, "Layout load in progress!  Next action in %d game turns\n", mTurnsToNext);
    for (const char* text : kDictionary) CTRACE(2, text);
    LLCM_SetIllegalPlacement(0);
    const int32_t type = static_cast<int32_t>(mReplayType);
    if (type == 0) {
        int32_t result;
        do {
            result = ReadAndProcessNextAction();
        } while (result != 0);
        EndReplay();
    } else if (type >= 0 && type < 3) {
        bool played = false;
        const uint32_t turn = gGameWorld->mTurn;
        while (mTurnsToNext == 0) {
            played = true;
            CTRACE(2, "Replaying a timed event at time %d\n", turn);
            if (ReadAndProcessNextAction() == 0) {
                EndReplay();
                break;
            }
            GetNextTimingFileEntry();
        }
        mTurnsToNext = mTurnsToNext - 1;
        if (mReplayType == 1 && played && static_cast<uint32_t>(mTurnsToNext) > 2) mTurnsToNext = 2;
    }
}

void CActionRecorder::GetNextTimingFileEntry() {}
// 0x10145360, 0x10145318 and 0x10143cac: the Mac's bodies only set r3 (1, 1 and 0); the host declares them void.
void CActionRecorder::GetNextCameraFileEntry() {}

// CActionRecorder::ReadAndProcessNextAction (main app 0x101453a8, 1856 bytes; unit v5077).

// Reads the next recorded action from the action file: one byte (the action), then the operands its kind (mActionKind[action]) needs.
// The file holds the operands little-endian (the Mac swaps them); the host reads them straight into native values. Any short read
// answers 0 (the recording has ended); otherwise the matching ProcessAction runs and the answer is 1. A kind above 11 traces an error
// and answers 1. The original reads the wide text of kinds 9 and 10 into a stack buffer that a count above 40 (kind 9) or 72 (kind 10)
// overruns; the host buffers hold the 255 characters.
int32_t CActionRecorder::ReadAndProcessNextAction() {
    using bfl::FileSystem::CFile;
    CFile* file = reinterpret_cast<CFile*>(mActionFile);
    auto read = [file](void* dest, uint32_t size) { return static_cast<uint32_t>(file->Read(dest, size)) == size; };

    uint8_t action;
    if (!read(&action, 1)) return 0;
    const int32_t id = action;

    switch (mActionKind[action]) {
        case 0: {   // 0x101454e0
            uint8_t b, a;
            if (!read(&b, 1)) return 0;
            if (!read(&a, 1)) return 0;
            ProcessAction(id, b, a);
            break;
        }
        case 1: {   // 0x10145554
            uint16_t a;
            if (!read(&a, 2)) return 0;
            ProcessAction(id, a);
            break;
        }
        case 2: {   // 0x1014542c
            uint8_t b, a;
            uint16_t c;
            if (!read(&b, 1)) return 0;
            if (!read(&a, 1)) return 0;
            if (!read(&c, 2)) return 0;
            ProcessAction(id, b, a, c);
            break;
        }
        case 3: {   // 0x101455a8
            uint16_t a;
            uint8_t b;
            if (!read(&a, 2)) return 0;
            if (!read(&b, 1)) return 0;
            ProcessAction(id, a, b);
            break;
        }
        case 4: {   // 0x1014562c
            float f;
            uint16_t a;
            if (!read(&f, 4)) return 0;
            if (!read(&a, 2)) return 0;
            ProcessAction(id, f, a);
            break;
        }
        case 5: {   // 0x101456c8
            float f, g;
            uint16_t a;
            if (!read(&f, 4)) return 0;
            if (!read(&g, 4)) return 0;
            if (!read(&a, 2)) return 0;
            ProcessAction(id, f, g, a);
            break;
        }
        case 6: {   // 0x101457ac
            uint16_t a, b;
            if (!read(&a, 2)) return 0;
            if (!read(&b, 2)) return 0;
            ProcessAction(id, a, b);
            break;
        }
        case 7: {   // 0x10145840
            uint16_t a, b, c;
            if (!read(&a, 2)) return 0;
            if (!read(&b, 2)) return 0;
            if (!read(&c, 2)) return 0;
            ProcessAction(id, a, b, c);
            break;
        }
        case 8:     // 0x10145418
            ProcessAction(id);
            break;
        case 9: {   // 0x10145914
            uint8_t count;
            char16_t text[256];
            if (!read(&count, 1)) return 0;
            if (!read(text, static_cast<uint32_t>(count) * 2)) return 0;
            SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(text), count);
            ProcessAction(id, count, text);
            break;
        }
        case 10: {  // 0x101459a4
            uint16_t a;
            uint8_t count;
            char16_t text[256];
            if (!read(&a, 2)) return 0;
            if (!read(&count, 1)) return 0;
            if (!read(text, static_cast<uint32_t>(count) * 2)) return 0;
            SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(text), count);
            ProcessAction(id, a, count, text);
            break;
        }
        case 11: {  // 0x10145a74
            uint8_t data[128];
            if (!read(data, 0x80)) return 0;
            ProcessAction(id, data);
            break;
        }
        default:
            CTRACE(2, "Erroneous stuff going down\n");
            break;
    }
    return 1;
}

// CActionRecorder::ProcessAction overloads (unit v5254): replay one recorded action by calling the game function it stands for.

// Untranslated callees (stand-ins in the launcher).
void SetPlaceNormalPylon();                                   // 0x10058b68
void SetPlaceSpecialPylon();                                  // 0x10058bb8
void LayoutRegenerateTrack();                                 // 0x10058e48
void LayoutDeleteLastTrackBrace();                            // 0x10058f4c
void LayoutDeleteMultiPylons(uint16_t a, uint16_t b);         // 0x10058cf8
void LayoutReplayCoasterSpeed(uint32_t a, uint32_t b);        // 0x10059120
void LayoutSetMovePylon(uint16_t a, uint16_t b, uint16_t c);  // 0x10058da4

#define AR_ERR "Erroneous stuff going down\n"

// 0x10145b28
void CActionRecorder::ProcessAction(int32_t action) {
    CTRACE(2, "AR replay - %d\n", action);
    switch (action) {
        case 0x37: SetPlaceNormalPylon(); break;
        case 0x38: SetPlaceSpecialPylon(); break;
        case 0x40: LayoutRegenerateTrack(); break;
        case 0x41: LayoutDeleteLastTrackBrace(); break;
        default: CTRACE(2, AR_ERR); break;
    }
}

// 0x10145d34
void CActionRecorder::ProcessAction(int32_t action, uint8_t a, uint8_t b, uint16_t c) {
    CTRACE(2, "AR replay - %d (%d, %d, %d)\n", action, a, b, c);
    if (action == 0x2d) LLCM_LeftMouseButtonUp(a, b, c, 0);
    else if (action == 0x28) LLCM_LeftMouseButtonDown(a, b, c, 0);
    else CTRACE(2, AR_ERR);
}

// 0x10145c24
void CActionRecorder::ProcessAction(int32_t action, uint8_t a, uint8_t b) {
    CTRACE(2, "AR replay - %d (%d, %d)\n", action, a, b);
    if (action == 0x14) LLCM_LeftMouseButtonUp(a, b, 0, 0);
    else if (action == 0xf) LLCM_LeftMouseButtonDown(a, b, 0, 0);
    else CTRACE(2, AR_ERR);
}

// 0x10146058
void CActionRecorder::ProcessAction(int32_t action, uint16_t a, uint8_t b) {
    CTRACE(2, "AR replay - %d (%d, %d)\n", action, a, b);
    if (action == 5) {
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
        id.mIndex = a;
        LLCC_SystemSetRide(id, b);
    } else {
        CTRACE(2, AR_ERR);
    }
}

// 0x1014634c
void CActionRecorder::ProcessAction(int32_t action, uint16_t a, uint16_t b) {
    CTRACE(2, "AR replay - %d (%d, %d)\n", action, a, b);
    if (action == 0x45) LayoutReplayCoasterSpeed(a, b);
    else if (action == 0x3f) LayoutDeleteMultiPylons(a, b);
    else CTRACE(2, AR_ERR);
}

// 0x1014644c
void CActionRecorder::ProcessAction(int32_t action, uint16_t a, uint16_t b, uint16_t c) {
    CTRACE(2, "AR replay - %d (%d, %d, %d)\n", action, a, b, c);
    if (action == 0x3c) LayoutSetMovePylon(a, b, c);
    else CTRACE(2, AR_ERR);
}

// 0x10145e38 (472 bytes): replays an action that carries one halfword; a jump table on the action (0 to 0x44), anything above traces an error.
void CActionRecorder::ProcessAction(int32_t action, uint16_t a) {
    uint32_t number = static_cast<uint32_t>(action);
    CTRACE(2, "AR replay - %d (%d)\n", action, static_cast<uint32_t>(a));
    if (number > 0x44) {
        CTRACE(2, "Erroneous stuff going down\n");
        return;
    }
    switch (number) {
        case 0x00: LLCC_SystemSetMode(a, false); break;
        case 0x19: LLCC_SystemSetModeNR(a, false); break;
        case 0x1e: LLCC_SystemSetRideTrack(a); break;
        case 0x23: LLCC_SystemSetRideQueue(a); break;
        case 0x0a: LLCC_SetRideRotation(a, false); break;
        case 0x32: {
            // the thing chain of the map cell: the first ride object (kind 3) becomes the selected object
            uint16_t id = gGameWorld->mMapCells[static_cast<int32_t>(a) - 1].mWho;
            while (id != gReplayChainEndA) {
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
                CThing* thing = static_cast<CThing*>(gThingArray[id].mThing);
                if (thing->mKind == 3) {
                    gUIInterface.SetSelectedObject(reinterpret_cast<CObject*>(thing));
                    LLCM_SetCurrentObject(false);
                    break;
                }
                id = thing->mCellNext;
            }
            break;
        }
        case 0x3d: LayoutStackPylonUp(a); break;
        case 0x3e: LayoutStackPylonDown(a); break;
        case 0x42: LayoutSetCoasterEditOld(a); break;
        case 0x44: LayoutSetCoasterEdit(a); break;
        case 0x43: LayoutRegenerateTrack(a); break;
        default: CTRACE(2, "Erroneous stuff going down\n"); break;
    }
}

// 0x101465a8 (604 bytes): the sign texts of a ride (actions 0x50 and 0x55): the text is swear-filtered and set as sign A (0x50) or B (0x55).
void CActionRecorder::ProcessAction(int32_t action, uint16_t a, uint8_t count, char16_t* text) {
    CTRACE(2, "AR replay - %d (%d, %d, %ls)\n", action, static_cast<uint32_t>(a), static_cast<uint32_t>(count), text);
    if (action != 0x55 && action != 0x50) return;
    std::u16string str(text, count);
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
    reinterpret_cast<SwearFilter*>(gMasterSave)->Filter(str, true);
    CThing* thing = nullptr;
    if (a == 60000) {
        uint16_t id = gGameWorld->mGatesThing;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        thing = static_cast<CThing*>(gThingArray[id].mThing);
    } else {
        uint16_t id = gGameWorld->mMapCells[static_cast<int32_t>(a) - 1].mWho;
        while (id != gReplayChainEndB) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            thing = static_cast<CThing*>(gThingArray[id].mThing);
            if (thing->mKind == 3) break;
            id = thing->mCellNext;
        }
    }
    CObject* object = reinterpret_cast<CObject*>(thing);
    char16_t buffer[100];
    if (action == 0x50) {
        object->SetSignNameA(&str[0]);
        object->GetSignNameB(buffer, 100);
        RegenerateRideSign(object, str.c_str(), buffer);
    } else if (action == 0x55) {
        object->SetSignNameB(&str[0]);
        object->GetSignNameA(buffer, 100);
        RegenerateRideSign(object, buffer, str.c_str());
    }
}

// 0x10146140
void CActionRecorder::ProcessAction(int32_t action, float a, uint16_t b) {
    CTRACE(2, "AR replay - %d (%g, %d)\n", action, static_cast<double>(a), b);
    if (action == 0x3a) LayoutSetRotatePylon(a, b);
    else if (action == 0x39) LayoutSetLoftPylon(a, b);
    else CTRACE(2, AR_ERR);
}

// 0x10146248
void CActionRecorder::ProcessAction(int32_t action, float a, float b, uint16_t c) {
    CTRACE(2, "AR replay - %d (%g, %g, %d)\n", action, static_cast<double>(a), static_cast<double>(b), c);
    if (action == 0x3b) LayoutSetWobblePylon(a, b, c);
    else CTRACE(2, AR_ERR);
}

// 0x10146524: the replay of an action that is no longer used; only the trace (the text is not read).
void CActionRecorder::ProcessAction(int32_t action, uint8_t count, char16_t* text) {
    CTRACE(2, "AR replay - %d (%d, %d) - NO LONGER USED\n", action, count);
}

namespace {
struct WideRep {
    uint32_t mF00;       // 0x00
    uint32_t mF04;       // 0x04
    int32_t mRefs;       // 0x08 the share count (0 is taken as 1)
    Ptr32<void> mBuffer; // 0x0c
};
struct WideString {
    Ptr32<WideRep> mRep; // 0x00
};
}  // namespace

// 0x101468f8: gives up the share; the last owner frees the buffer and the representation.
void WideStringDestroy(void* string, int16_t flag) {
    WideString* self = static_cast<WideString*>(string);
    if (self == nullptr) return;
    WideRep* rep = self->mRep.get();
    if (rep->mRefs == 0) rep->mRefs = 1;
    int32_t left = rep->mRefs - 1;
    rep->mRefs = left;
    if (left != 0) self->mRep.mOffset = 0;
    if (self->mRep.get() != nullptr) {
        v51072_callee::OperatorDelete(self->mRep->mBuffer.get());
        v51072_callee::OperatorDelete(self->mRep.get());
    }
    if (flag > 0) v51072_callee::OperatorDelete(self);
}

// 0x101437e0
void CMemFileDestroy(void* file, int16_t flag) {
    bfl::FileSystem::CMemFile* self = static_cast<bfl::FileSystem::CMemFile*>(file);
    if (self == nullptr) return;
    self->bfl::FileSystem::CFile::~CFile();
    if (flag > 0) v51072_callee::OperatorDelete(self);
}

// 0x10146850 (96 bytes): the buffer is not read.
void CActionRecorder::ProcessAction(RecordedAction action, uint8_t* data) {
    static_cast<void>(data);
    CTRACE(2, "AR replay - %d (128 bytes of nothing)\n", static_cast<int32_t>(action));
    if (static_cast<int32_t>(action) != 0xe7) CTRACE(2, "Erroneous stuff going down\n");
}

// The callers (and the test libraries' fakes) pass the action as an int, the header's first overloads; the Mac has one function per
// signature, taking a RecordedAction (0x10144bec, 0x101441fc, 0x101445d0, 0x10144704 and, for the replay, 0x10146850). These reach them.
void CActionRecorder::RecordAction(int32_t action, uint8_t* data) { RecordAction(static_cast<RecordedAction>(action), data); }
void CActionRecorder::RecordAction(int32_t action, uint16_t value) { RecordAction(static_cast<RecordedAction>(action), value); }
void CActionRecorder::RecordAction(int32_t action, uint16_t value, uint8_t extra) {
    RecordAction(static_cast<RecordedAction>(action), value, extra);
}
void CActionRecorder::RecordAction(int32_t action, float value, uint16_t id) { RecordAction(static_cast<RecordedAction>(action), value, id); }
void CActionRecorder::ProcessAction(int32_t action, uint8_t* data) { ProcessAction(static_cast<RecordedAction>(action), data); }

// Unit v51048: eight small functions of the main app (Game.cpp, ActionRecorder.cpp, ui_visitorStatus.cpp's list instance, Peep.cpp's park time,
// UI_Base.cpp's bar and deferred message).

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// Raw storage (the Mac's string is 0x14 bytes, the recorder's records unknown); names invented.
alignas(16) uint8_t gActionRecorderString[64];        // 0x1037f024
alignas(16) uint8_t gActionRecorderStringNode[32];    // 0x1037f038
alignas(16) uint8_t gActionRecorderNode[32];          // 0x1037f018
alignas(16) uint8_t gActionRecorderDtor[16];          // stands for the destructor word 0x10266e58

static const char16_t kActionRecorderText[] = u"gms.dat";   // the text at 0x10248615

// 0x10146e9c (88 bytes): the file's static initialiser: a wide string built from the text and registered for exit with the string destructor,
// then the action recorder constructed and registered with its own destructor.
void __sinit_ActionRecorder_cpp() {
    void* str = ::new (gActionRecorderString) TbDynamicStringTemplate<char16_t>(kActionRecorderText);
    __register_global_object(str, reinterpret_cast<void*>(&UIMain_DestroyWString), gActionRecorderStringNode);
    gActionRecorder.Construct();
    __register_global_object(&gActionRecorder, gActionRecorderDtor, gActionRecorderNode);
}
