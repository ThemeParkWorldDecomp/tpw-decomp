// ActionRecorder.cpp (main app), unit v5362: ProcessAction 0x10145e38 (one halfword), ProcessAction 0x101465a8 (halfword, count, text) and
// RecordAction 0x1014445c (three halfwords).
#include <cstdint>
#include <string>

#include "ActionRecorder.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "bfl_FileSystem.h"
#include "ui_main.h"

// Names invented; initialised data of the main app, both 0.
uint16_t gReplayChainEndA = 0;   // 0x102a4bd4
uint16_t gReplayChainEndB = 0;   // 0x102a4cec

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
