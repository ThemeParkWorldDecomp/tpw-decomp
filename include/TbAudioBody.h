// TbAudioBody.h: the audio object the game holds (unit w79f, "sound symbols shared", Code at 0x10000000, TOC r2 = 0x1003abf0).
// CreateAudioObject (0x1002a18c) makes one and SOUND_Init stores it in gAudioSystem; the game only calls it through the abstract
// TbAudioSystem (include/TbAudioSystem.h) whose slots it fills. The body owns the whole sound path: the head of a component
// chain (a plain CAudioComponent), the sound manager at its tail (TbSoundManager over TbSoundCore, which owns the device module
// and the sample store), the pool of playing sounds (CAudioPlaceHolderList) and the listener.
//
// Mac layout, 0x40 bytes: +0 vptr (0x1003cb70), +4 mForward, +0x10 mUp, +0x1c mVelocity (TbSoundPosition, 12 bytes each),
// +0x28 mPlaceHolders (CAutoPtr, the pointer), +0x2c mManager, +0x30 mComponent, +0x34 mPosition. The host's pointers are native,
// so the host object is larger; only the member order matters.
//
// Vtable 0x1003cb70 fills the slots of TbAudioSystem (0x1003cbb0): +0x08 Release 0x10016804, +0x0c SetListenerOrientation
// 0x10017610, +0x10 RegisterCatagory 0x10016ae4, +0x14 SendEvent 0x10016c1c, +0x18 UpdateHandle 0x10017314, +0x1c UpdateParameter
// 0x100173d4, +0x20 SystemCommand 0x1001753c, +0x24 Init 0x1001653c, +0x28 UpdateDistortion 0x1001781c, +0x2c UpdateLowPass
// 0x1001785c, +0x30 the destructor 0x100163bc, +0x34 StopHandle 0x10017490, +0x38 DeregisterCatagoryBanks 0x10016884,
// +0x3c CreateSystemChain 0x10016480.
//
// Calling convention: a function returning TbError returns it through a hidden r3 buffer on the Mac (this moves to r4); the host
// returns TbError by value. SendEvent, UpdateHandle and UpdateParameter answer one word in r3 (the host's int32_t).
#pragma once
#include <cstdint>

#include "AudioPlaceHolderList.h"
#include "TbAudioSystem.h"
#include "TbDynamicStringTemplate.h"
#include "TbSoundCore.h"
#include "TbSoundManager.h"
#include "snd_containers.h"

// Makes the two kinds of component the body chains (unit w79f; the class is an empty bag of statics in the library's LbMpeg.cpp).
class CAudioComponentFactory {
public:
    static TbSoundCore* CreateCore();            // 0x10001b54 (72): new (0x24) TbSoundCore, or NULL when the allocation fails
    static CAudioComponent* CreateComponent();   // 0x10001bd8 (72): new (0x18) CAudioComponent, or NULL
};

// 0x10016a84 (64, `_IsSlash` in the library): '\\', '/' or ':' (the volume separator), compared as a signed char
bool IsSlash(char c);

class TbAudioBody : public TbAudioSystem {
public:
    TbAudioBody();                  // 0x100161a0 (168)
    ~TbAudioBody() override;        // 0x100163bc (156), slot +0x30

    // 0x10016804 (84), slot +0x08: deletes the object itself (the Mac tests its pointer against NULL first); answers 0
    TbError Release() override;
    // 0x10017610 (296), slot +0x0c: when any of the four differs from the stored one, stores all four and passes the stored ones
    // to the manager; -1 without a manager
    TbError SetListenerOrientation(const TbSoundPosition& position, const TbSoundPosition& forward, const TbSoundPosition& up,
                                   const TbSoundPosition& velocity) override;
    // 0x10016ae4 (184), slot +0x10: handle = 0; -1 without a manager; else the streamer registers the qualified path
    TbError RegisterCatagory(const char* path, const char* name, uint32_t& handle) override;
    // 0x10016c1c (360), slot +0x14: answers the sound's handle (the entity itself when nothing new is started)
    int32_t SendEvent(int32_t entity, const TbAudioEvent& event) override;
    // 0x10017314 (104), slot +0x18: 0 for a NULL handle
    int32_t UpdateHandle(int32_t handle, const TbSoundPosition& position, const TbSoundPosition& velocity) override;
    // 0x100173d4 (132), slot +0x1c: 0 for a NULL handle or without a list
    int32_t UpdateParameter(int32_t handle, uint8_t parameter, uint32_t value) override;
    // 0x1001753c (136), slot +0x20: the command's Execute with this body; -1 without a manager or a list
    TbError SystemCommand(const TbSysCommand::Command& command) const override;
    // 0x1001653c (316), slot +0x24: builds the chain (the virtual CreateSystemChain), starts the components and the manager, and
    // wires the manager to the streamer, the listener and the placeholders; -1 when a start fails
    TbError Init(const char* name, const CAudioConfig& config) override;
    // 0x1001781c (8) and 0x1001785c (8), slots +0x28 and +0x2c: on the Mac `return handle` in r3 (a plain word, not a hidden
    // TbError); the host's declaration answers a TbError, so the handle comes back as its value
    TbError UpdateDistortion(void* handle, float value) override;
    TbError UpdateLowPass(void* handle, float value) override;
    // 0x10017490 (124), slot +0x34: the placeholder's Stop; -1 for a NULL or unknown handle
    TbError StopHandle(void* handle) const override;
    // 0x10016884 (100), slot +0x38: -1 for category 0, else the streamer's RemoveCatagory
    TbError DeregisterCatagoryBanks(uint32_t category) const override;
    // 0x10016480 (120), slot +0x3c: a plain component at the head and a new manager at the tail
    TbError CreateSystemChain(const CAudioConfig& config) override;

    // 0x10016928 (208): `out` = `path` when it names a drive ("C:\x") or a network path ("\\x"), else the sound path plus `path`
    void QualifyPath(const char* path, TbDynamicStringTemplate<char>& out);
    // 0x10016f68 (32): true when the event flags have neither bit 4 nor bit 8
    bool CanBeDroppedDueToVolControl(uint32_t flags);
    TbSoundManager* GetSoundManager() const;            // 0x10028110 (8)
    CAudioPlaceHolderList* GetPlaceHolderList() const;   // 0x1002814c (36)

    TbSoundPosition mForward;                  // +0x04
    TbSoundPosition mUp;                       // +0x10
    TbSoundPosition mVelocity;                 // +0x1c
    CAutoPtr<CAudioPlaceHolderList> mPlaceHolders;   // +0x28
    TbSoundManager* mManager;                  // +0x2c
    CAudioComponent* mComponent;               // +0x30
    TbSoundPosition mPosition;                 // +0x34
};

// The library's TpBody (CSprocketsModule.cpp, after the TbSysCommand bodies): the class the Tick, Save and Load commands call with
// the body they are given (the members read the body's layout: the position at +0x34 and the list at +0x28). Unit w79h only declares
// the three members the commands use; they are stand-ins until a unit translates them. 0x1002e404 (36) calls CSprocketsModule::Tick;
// 0x1002e57c (264) writes the listener's position, then the list (-1 without a list); 0x1002e6ec (328) reads them back.
class TpBody {
public:
    void Tick();                                  // 0x1002e404 (36)
    TbError Save(void* file) const;               // 0x1002e57c (264)
    // 0x1002e6ec (328): the flag is the command's byte as it comes (the Mac passes it unchanged; the library's own type is bool)
    TbError Load(void* file, uint8_t flag) const;

    // Unit v51113: the stand-in virtuals. Init, Close, SetFades and SetCatagoryReverb answer 0 and read nothing.
    TbError Init(const char* name);                           // 0x1002e3bc (36)
    TbError Close();                                          // 0x1002e44c (36)
    TbError SetFades(uint32_t fades) const;                   // 0x1002e494 (36)
    TbError DeregisterCatagoryBanks(uint32_t category) const; // 0x1002e4e0 (100): -1 for category 0, else the streamer's RemoveCatagory
    TbError SetCatagoryReverb(uint32_t category) const;       // 0x1002e8a4 (36)
    // 0x1002e8fc (104): the destructor with its hidden delete flag; stores TpBody's vtable (0x1003e2c8, not modelled on the host: the
    // class has none), runs TbAudioBody's destructor, a flag above 0 frees through 0x1002ea94; answers this.
    static TpBody* Destruct(TpBody* self, int16_t deleteFlag);
};
