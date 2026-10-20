#include <cstdint>
#include "analyser.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "bfl_FileSystem.h"

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

// 0x10146850 (96 bytes): the buffer is not read.
void CActionRecorder::ProcessAction(RecordedAction action, uint8_t* data) {
    static_cast<void>(data);
    CTRACE(2, "AR replay - %d (128 bytes of nothing)\n", static_cast<int32_t>(action));
    if (static_cast<int32_t>(action) != 0xe7) CTRACE(2, "Erroneous stuff going down\n");
}
