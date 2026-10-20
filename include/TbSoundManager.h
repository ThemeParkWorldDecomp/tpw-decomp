// TbSoundManager.h: the sound manager the game holds (unit w79e, "sound symbols shared", Code at 0x10000000, TOC r2 =
// 0x1003abf0). It derives from TbSoundCore (include/TbSoundCore.h) and adds the game-facing surface: sample playing, bank
// access, the four volume controls, the listener and the QSound flags. The static pointer to the one manager is
// TbMapStreamer::mpSoundManager (sound 0x1003f3b4, include/TbMapStreamer.h), which stores it in SetSoundManager.
//
// Mac layout, 0x68 bytes (TbSoundCore is the first 0x24): +0x24 mOptions (8 from the constructor: bit 8 QSound on, bit 0x100 set
// by SetQSoundInfo's second flag), +0x28 mDistanceMultiplier (1.0f), +0x2c mSoundPath (a TbDynamicStringTemplate<char>, 0x14
// bytes), +0x40 mListenerPosition (a POINTER to the caller's object), +0x44 mAudioSystem (the component the samples are made
// through), +0x48 four bytes mControlOn (1 each), +0x4c four floats mVolumes (1.0f each), +0x5c mVolumeControl (8 bytes: Init(4)
// then SetVolumes(mVolumes)), +0x64 mDroppable (a byte, 0), three bytes of padding. The host's pointers are native, so the
// host object is larger; only the order of the members matters (UpdateVolumeControl and GetVolume index mVolumes without a bounds
// check, as the original does: an index of 4 or more writes into the members that follow it).
//
// Vtable 0x1003d668 (the constructor's `addi r31, r2, 0x2a78`): +0x08..+0x20 the seven access slots of TbSoundCore's table
// (nothing is overridden: GetSystemInfo 0x1000bc8c, GetSampleInfo 0x1000c058, NewSample 0x1000c220, NewControl 0x1000c404,
// SystemCommand 0x1000c470, GetSystemModule 0x1000c500, GetSampleStore 0x1000c53c), +0x24 the TbRefRelease sub-table (rtti, -4,
// Release, AddRef, a this-adjusting thunk 0x10027fcc into the destructor), +0x38 TbSoundCore::Init 0x1000b85c (NOT overridden),
// +0x3c ~TbSoundManager 0x10027f20, then the manager's own new virtuals: +0x40 Init(const char*) 0x10026fa4 and +0x44
// GetRolloffFactorToMinMaxDistance 0x1001960c. The word at +0x48 (0x1002fee9, the name "TbSysCommand::Command") is the first word of the next vtable (that class's RTTI), not a slot.
// The secondary vptr at this+4 is 0x1003d68c (the TbRefRelease part of the same table).
//
// Calling convention: every function returning TbError returns it through a hidden r3 buffer on the Mac (this moves to r4; the
// listing's parameter 1 is the real this). The host returns TbError by value. A virtual call on the Mac is `lwz r12, 0(this);
// lwz r12, SLOT(r12); bl 0x1002f154` (the pointer glue).
#pragma once
#include <cstdint>

#include "TbDynamicStringTemplate.h"
#include "TbError.h"
#include "TbSoundCore.h"
#include "TbSoundInfo.h"
#include "TbSoundPosition.h"

class TbBank;
class CBankContainer;

class TbSoundManager : public TbSoundCore {
public:
    TbSoundManager(); // 0x10026e30 (204)
    ~TbSoundManager() override; // 0x10027f20 (128): the string, then ~TbSoundCore; the deleting form is the virtual +0x3c

    // The core's Init(const TbSoundSystemInfo&) stays reachable under the same name (the game calls it separately: the
    // manager's Init does not).
    using TbSoundCore::Init;
    // 0x10026fa4 (76), slot +0x40: mSoundPath = path (the string's virtual operator=(const char*), slot +0x20); answers 0. It
    // does NOT call TbSoundCore::Init.
    virtual TbError Init(const char* path);
    // 0x1001960c (24), slot +0x44: (min / (1/64) - min) / (max - min), single precision.
    virtual float GetRolloffFactorToMinMaxDistance(float min, float max);

    TbDynamicStringTemplate<char>& GetSoundPath(); // 0x10016a48 (8): &mSoundPath
    void SetListenerPosition(const TbSoundPosition& position); // 0x1001670c (8): keeps the address
    const TbSoundPosition* GetListenerPosition(); // 0x10017cf4 (8)
    void SetAudioSystem(CAudioComponent* component); // 0x100167b4 (8)
    void SetDistanceMapping(float multiplier); // 0x10027274 (8)
    float GetDistanceMultiplier(); // 0x1001966c (8)
    uint32_t IsQSoundOn(); // 0x1001954c (28): (mOptions & 8) != 0 as 1 or 0
    TbSoundVolumeControl* GetVolumeControl(); // 0x10019758 (8): &mVolumeControl
    float GetVolume(uint8_t index); // 0x1002a080 (16): mVolumes[index], no bounds check
    // 0x10016e60 (204): true unless a control selected by `mask` (bits 1, 2, 4, 8 for controls 0..3) is off. Higher bits ignored.
    bool VolumeControlIsOn(uint8_t mask);
    // 0x100279c4 (100): mVolumes[index] = volume (NO bounds check), then SystemCommand(UpdateAllVolumes()).
    TbError UpdateVolumeControl(uint8_t index, float volume);
    // 0x10027adc (68): index < 4: mControlOn[index] = (byte)on, answer 0; else -1. No command is sent.
    TbError ToggleVolumeControl(uint8_t index, uint32_t on);
    // 0x10027160 (216): enable == 0 clears option bit 8, else sets it. A fresh TbSoundSystemInfo gets request bit 4 and option
    // bit 0x100 when `extended` is nonzero, and request bit 8 when `third` is nonzero (the request word is mField4); then
    // SystemCommand(SetSystemInfo(info)).
    TbError SetQSoundInfo(uint32_t enable, uint32_t extended, uint32_t third);
    // 0x10027b60 (168): a fresh info; mListenerPosition = &position; the info's SetListenerPosition(position),
    // SetListenerDirection(front, top), SetListenerVelocity(velocity); SystemCommand(SetSystemInfo(info)).
    TbError SetListenerOrientation(const TbSoundPosition& position, const TbSoundPosition& front, const TbSoundPosition& top,
                                   const TbSoundPosition& velocity);
    // 0x10027020 (192): request bit 0x80 (mField8); GetSystemInfo(info); on success and with bit 0x80 in the answered mask:
    // out = info.GetMaxBuffers(), answer 0; else -1.
    TbError GetMaxNumberOfChannels(uint32_t& out);
    // 0x100278c0 (196): out = 0; request bit 1 (mField8); GetSystemInfo(info); on success and with bit 1 in the mask:
    // out = info.mField0 (the IDirectSound slot), answer 0; else -1.
    TbError GetDirectSoundObject(uint32_t& out);
    // 0x10027334 (100): the streamer's bank container number `index`, or NULL out of range.
    CBankContainer* GetBankData(uint32_t index);
    bool IsBankDroppable(uint32_t index); // 0x100272b8 (68): the container's flag 0x40
    // 0x10027408 (80): on a change, mDroppable = on and TbMapStreamer::Instance()->SetDroppable(mDroppable).
    void SetDroppable(bool on);
    // 0x1002748c (776): the bank of container `index`, made on the first call (CFileOperation::GetBankPath, then the container's
    // storage opens the file and CBankFactory::GenerateBank makes the bank, kept in the container); NULL when the container,
    // the path or the open fails.
    TbBank* GetBank(uint32_t index);
    // 0x10027c90 (180): control = mAudioSystem->NewControl(handle, info) (slot +0x14); NULL when info.mFlags has 0x2000 and
    // there is no control; else mAudioSystem->NewSample(control ? control : handle, info) (slot +0x10). mAudioSystem must be set.
    void* ConstructSample(void* handle, void*& control, TbSoundSampleInfo& info);
    // 0x10027d94 (216): info.mField0C |= 2 when both option bits 8 and 0x100 are set, |= 0x2000 always (the word at +0xc, while
    // ConstructSample tests mFlags at +0x14: a quirk of the original); sample = ConstructSample(NULL, control, info); then, when
    // there is a sample, control = sample if there is none, SystemCommand(Start(control)); the sample on success, else NULL.
    void* PlaySample(TbSoundSampleInfo& info, void*& control);

    uint32_t mOptions; // +0x24
    float mDistanceMultiplier; // +0x28
    TbDynamicStringTemplate<char> mSoundPath; // +0x2c
    const TbSoundPosition* mListenerPosition; // +0x40
    CAudioComponent* mAudioSystem; // +0x44
    uint8_t mControlOn[4]; // +0x48
    float mVolumes[4]; // +0x4c
    TbSoundVolumeControl mVolumeControl; // +0x5c
    uint8_t mDroppable; // +0x64
};
