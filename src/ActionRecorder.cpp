// ActionRecorder.cpp of the main app (SimTheme Park 2.04b3), unit w133: CActionRecorder::ResetActionRecording (0x10143e08, 260 bytes) and
// CActionRecorder::DoTick (0x10144f80, 872 bytes).
#include "ActionRecorder.h"

#include <cstring>
#include <new>

#include "Debug.h"
#include "LLCellSystem.h"
#include "World.h"
#include "bfl_FileSystem.h"

using bfl::FileSystem::CMemFile;

namespace {

// The recorder's three files. On the Mac they are CMemFile objects inside the recorder, made at start-up; here they are storage that is
// made into a CMemFile on first use (a zeroed object has no vtable word).
CMemFile& FileOf(uint8_t* storage) {
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
    FileOf(mActionFile).Close();
    if (mMode == 0) {
        FileOf(mTimingFile).Close();
        FileOf(mCameraFile).Close();
    }
    const int32_t mode = mMode;
    mState = 1;
    static_cast<void>(FileOf(mActionFile).Open(0x200));
    if (mode == 0) {
        static_cast<void>(FileOf(mTimingFile).Open(0x200));
        static_cast<void>(FileOf(mCameraFile).Open(0x200));
    }
    for (int32_t i = 0; i < 0x80; ++i) buffer[i] = 0;
    RecordAction(0xe7, buffer);
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
