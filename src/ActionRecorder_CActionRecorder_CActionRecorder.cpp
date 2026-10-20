// ActionRecorder_CActionRecorder_CActionRecorder.cpp (main app): CActionRecorder constructor 0x10143658, destructor 0x10143880,
// Serialise 0x101439dc and RecordAction 0x1014430c (unit v5075).
#include <cstdint>
#include <new>

#include "ActionRecorder.h"
#include "Debug.h"
#include "LbFile.h"
#include "Serialise.h"
#include "bfl_FileSystem.h"

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
