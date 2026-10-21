// TbSoundCore.h: the sound library's core (unit w79b, "sound symbols shared", Code at 0x10000000, TOC r2 = 0x1003abf0):
// TbSoundCore, the component chain it hangs on (CAudioComponentAccess, CAudioComponent), the device-module factory
// (CModuleFactory) and CMemoryCopyInit. Functions are in src/sound/TbSoundCore.cpp; the command constructors are in src/sound/TbSysCommand.cpp, src/sound/TbSoundCore.cpp, src/sound/AudioPlaceHolder.cpp.
//
// Calling convention: every function returning TbError returns it through a hidden r3 result buffer on the Mac (this moves to
// r4); the host returns TbError by value (see include/TbSoundSystemModule.h). A virtual call on the Mac goes through the
// PowerPC pointer-glue thunk 0x1002f154 with the slot read from the object's vtable.
//
// Mac vtable of TbSoundCore, 0x1003c120, one contiguous table: +0x00 rtti 0x1003c118, +0x04 0, +0x08 GetSystemInfo 0x1000bc8c,
// +0x0c GetSampleInfo 0x1000c058, +0x10 NewSample 0x1000c220, +0x14 NewControl 0x1000c404, +0x18 SystemCommand 0x1000c470,
// +0x1c GetSystemModule 0x1000c500, +0x20 GetSampleStore 0x1000c53c; then the TbRefRelease sub-table the base at this+4 points to
// (0x1003c144 = table + 0x24): +0x24 rtti, +0x28 offset -4, +0x2c Release (descriptor 0x1003f438, an import of the bullfrog
// TbRefRelease::Release), +0x30 AddRef (0x1003f3f0), +0x34 a this-adjusting thunk 0x1000c578 to the destructor; then the class's
// own virtuals: +0x38 Init 0x1000b85c, +0x3c the deleting destructor 0x1000b738. CAudioComponent's table 0x10035c24 has the same
// shape (sub-table vptr 0x10035c48; Init at +0x38 is descriptor 0x10033464, the destructor at +0x3c is 0x1003346c).
// CAudioComponentAccess's table is 0x10035c64 (all slots zero). On the host the bases are the C++ ones; the tests compare
// behaviour through calls, not vtable bytes.
#pragma once
#include <cstdint>
#include "TbError.h"
#include "TbRefRelease.h"
#include "TbSoundInfo.h"
// TbSoundCodecs.h (CMemoryCopy) is not included here: it reaches TbSoundPipe.h and TbSoundBank.h, which include this header.
// The files that use CMemoryCopy include it themselves.
#include "snd_commands.h"

class TbSoundSystemModule;
class TbSoundSampleStore;
class TbSoundPipeChannel;

// The pure interface (seven slots, +0x08 to +0x20). Constructor 0x100013f4 (12 bytes) stores the vptr.
class CAudioComponentAccess {
public:
    CAudioComponentAccess();
    virtual TbError GetSystemInfo(TbSoundSystemInfo& info) = 0; // +0x08
    virtual TbError GetSampleInfo(void* handle, TbSoundSampleInfo& info) = 0; // +0x0c
    virtual void* NewSample(void* handle, TbSoundSampleInfo& info) = 0; // +0x10
    virtual void* NewControl(void* handle, TbSoundSampleInfo& info) = 0; // +0x14
    virtual TbError SystemCommand(const SystemCommand::Command& command) = 0; // +0x18
    virtual TbSoundSystemModule* GetSystemModule() const = 0; // +0x1c
    virtual TbSoundSampleStore* GetSampleStore() const = 0; // +0x20
};

// A link in a chain of components; each forwards the access calls to the next one (no NULL test). Mac size 0x18: +0 vptr,
// +4 the TbRefRelease base (its vptr, count at +8), +0x0c mNext, +0x10 mPrevious, +0x14 mId. The chain holds a reference on the
// next component.
class CAudioComponent : public CAudioComponentAccess, public TbRefRelease {
public:
    CAudioComponent(); // 0x10001364 (100): the bases, then the three links zero
    ~CAudioComponent() override; // 0x10001434 (140): releases mNext; the deleting form deletes the object (slot +0x3c)
    // -1 without a next; otherwise the next's Init, and when that fails the next is unchained and released; the answer is 0
    // even then (quirk).
    virtual TbError Init(const TbSoundSystemInfo& info); // 0x100014ec (140), slot +0x38
    // Inserts `other` after this one (the old next is chained after `other` by the recursive call); -1 for NULL.
    TbError Chain(CAudioComponent* other); // 0x100015e4 (172)
    int32_t UnchainAndRelease(); // 0x100016d0 (108): takes this out of the chain and deletes it; answers 0
    CAudioComponent* GetComponent(uint32_t id); // 0x10001778 (68): this when mId matches, else along mNext
    TbError GetSystemInfo(TbSoundSystemInfo& info) override; // 0x100017f4 (60)
    TbError GetSampleInfo(void* handle, TbSoundSampleInfo& info) override; // 0x10001878 (60)
    void* NewSample(void* handle, TbSoundSampleInfo& info) override; // 0x10001900 (48)
    void* NewControl(void* handle, TbSoundSampleInfo& info) override; // 0x10001978 (48)
    TbError SystemCommand(const SystemCommand::Command& command) override; // 0x10001ac0 (60)
    TbSoundSystemModule* GetSystemModule() const override; // 0x100019f0 (48)
    TbSoundSampleStore* GetSampleStore() const override; // 0x10001a58 (48)

    CAudioComponent* mNext; // +0x0c
    CAudioComponent* mPrevious; // +0x10
    uint32_t mId; // +0x14
};

// The global copier: the 4-byte member makes the standard copier (CMemoryCopy::_mpCopy, 0x1003f34c) that the codecs use.
class CMemoryCopyInit {
public:
    CMemoryCopyInit(); // 0x10014b78 (88): _mpCopy = new CStandardCopy (stored even when the allocation answers NULL)
    // Raw operator delete of _mpCopy (not virtual; the static is not cleared). The deleting form also deletes the object.
    ~CMemoryCopyInit(); // 0x1000b6ac (96)
};

// An empty class used as a stack temporary.
class CModuleFactory {
public:
    // Makes a CSprocketsModule (0x18 bytes) into `module` and starts it with Init(info) (slot +0x20); on failure the module
    // is closed and deleted again and the answer is -1. `store` is not used (the original has it in r7 and never reads it).
    TbError GenerateModule(TbSoundSystemModule*& module, const TbSoundSystemInfo& info, TbSoundSampleStore* store); // 0x10006ff8 (176)
    void CloseModule(TbSoundSystemModule*& module); // 0x10007120 (108): Close() (+0x24), delete (+0x08, flag 1), NULL
};

// The hub of the runtime sound path: owns the device module and the sample store. Mac size 0x24: +0 the CAudioComponent base
// (0x18 bytes), +0x18 mModule, +0x1c mStore, +0x20 mCopyInit.
class TbSoundCore : public CAudioComponent {
public:
    TbSoundCore(); // 0x1000b618 (108)
    ~TbSoundCore() override; // 0x1000b738 (136): Close(), ~CMemoryCopyInit, ~CAudioComponent
    void ModifyMaxBuffers(TbSoundSystemInfo& info); // 0x1000b7e8 (44): info.SetMaxBuffers(info.mBuffersToUse)
    // Copies the info, makes the store (ctor args: this, the copy's max buffers) and starts it (the copy's buffers to use),
    // makes the module with CModuleFactory, then runs SetSystemInfo(copy) through SystemCommand. -1 when the store or the
    // module fails (the store is left made).
    TbError Init(const TbSoundSystemInfo& info) override; // 0x1000b85c (264), slot +0x38
    void Close(); // 0x1000bb50 (184)
    TbError GetSystemInfo(TbSoundSystemInfo& info) override; // 0x1000bc8c (260)
    TbError StopAndReleasePipe(TbSoundPipeChannel* channel, bool release); // 0x1000bf50 (188)
    TbError GetSampleInfo(void* handle, TbSoundSampleInfo& info) override; // 0x1000c058 (252)
    void* NewSample(void* handle, TbSoundSampleInfo& info) override; // 0x1000c220 (132)
    void* NewControl(void* handle, TbSoundSampleInfo& info) override; // 0x1000c404 (40)
    TbError SystemCommand(const SystemCommand::Command& command) override; // 0x1000c470 (68)
    TbSoundSystemModule* GetSystemModule() const override; // 0x1000c500 (8)
    TbSoundSampleStore* GetSampleStore() const override; // 0x1000c53c (8)

    TbSoundSystemModule* mModule; // +0x18
    TbSoundSampleStore* mStore; // +0x1c
    CMemoryCopyInit mCopyInit; // +0x20
};
