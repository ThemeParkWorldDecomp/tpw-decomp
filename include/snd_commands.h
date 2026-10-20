// snd_commands.h: the command objects the sound manager hands to TbSoundCore::SystemCommand (unit w79b, "sound symbols shared").
// Each is a vptr plus at most two words on the Mac (the host's pointer members are native). Execute is slot +0x08 of every vtable,
// declared `Execute(CAudioComponentAccess*) const` in the binary (mangled ...CFP21CAudioComponentAccess); it returns TbError
// through the hidden r3 buffer (this moves to r4, as in include/TbSoundSystemModule.h) and reads the command's words as
// *(command + 4) etc. The Execute bodies are unit w79d's: they are only declared here. The base's slot +0x08 is zero in its
// vtable (pure). TbSoundCore::SystemCommand calls command.Execute(this) with the core as its CAudioComponentAccess base.
// Constructor addresses, vtables (TOC 0x1003abf0 + the constructor's offset) and Execute addresses are in the comments.
#pragma once
#include <cstdint>
#include "TbError.h"
#include "TbSoundInfo.h"

class CAudioComponentAccess;

namespace SystemCommand {

class Command {
public:
    Command(); // 0x1000ba38 (12): vtable 0x1003c160
    virtual TbError Execute(CAudioComponentAccess* access) const = 0; // +0x08
};

// A command on one sample: +4 the sample handle.
class HandleCommand : public Command {
public:
    explicit HandleCommand(void* handle); // 0x1000c380 (72): Command(), vtable 0x1003c0c4, +4 = handle
    void* mHandle; // +4
};

// +4 the system info the core hands to the device (the caller's object, by pointer).
class SetSystemInfo : public Command {
public:
    explicit SetSystemInfo(TbSoundSystemInfo& info); // 0x1000b9a0 (72): Command(), vtable 0x1003cb28, +4 = &info
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x10015b8c (656)
    TbSoundSystemInfo* mInfo; // +4
};

// +4 the handle (HandleCommand), +8 the sample info (by pointer).
class SetSampleInfo : public HandleCommand {
public:
    SetSampleInfo(void* handle, TbSoundSampleInfo& info); // 0x1000c2e8 (72): HandleCommand(handle), vtable 0x1003c9f0, +8 = &info
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x1001515c (180)
    // Pushes the info to the device module and the channel at `index` (the channel's index in the store); answers 0. It
    // changes the caller's info: flag 0x400 is cleared after the priority is set, and the volume is overwritten by the
    // channel's controlled volume. Declared here by w79d (the binary's member, not in w79b's header).
    TbError SetChannelInfo(CAudioComponentAccess* access, uint32_t index) const; // unit w79d, 0x10015268 (432)
    TbSoundSampleInfo* mInfo; // +8
};

class Stop : public HandleCommand {
public:
    explicit Stop(void* handle); // 0x10019c34 (56): vtable 0x1003cae0
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x10015684 (188)
};

class Pause : public HandleCommand {
public:
    explicit Pause(void* handle); // 0x1001a60c (56): vtable 0x1003c9ac
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x10014e00 (184)
};

class Restart : public HandleCommand {
public:
    explicit Restart(void* handle); // 0x1001a748 (56): vtable 0x1003c964
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x10014fac (184)
};

class Start : public HandleCommand {
public:
    explicit Start(void* handle); // 0x10027eb4 (56): vtable 0x1003ca34
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x100154e4 (336)
};

// Constructor inlined elsewhere (not translated by w79b); Execute reads the handle at +4 like the other HandleCommands.
class StopLoop : public HandleCommand {
public:
    explicit StopLoop(void* handle) : HandleCommand(handle) {} // inlined at the callers on the Mac; declared by w79d
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x1001578c (224)
};

// Constructor inlined elsewhere (not translated by w79b); Execute reads one word at +4 (the volume control index).
class StopAllVolumeControl : public Command {
public:
    explicit StopAllVolumeControl(uint32_t mask) : mIndex(mask) {} // inlined at the callers on the Mac; declared by w79d
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x10015a24 (96)
    uint32_t mIndex; // +4
};

class UpdateAllVolumes : public Command {
public:
    UpdateAllVolumes(); // 0x10027a64 (56): vtable 0x1003cb08
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x10016050 (244)
};

class Process : public Command {
public:
    Process(); // 0x100280a4 (56): vtable 0x1003cb48
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x10015ae4 (88)
};

class StopAll : public Command {
public:
    StopAll(); // 0x10028270 (56): vtable 0x1003ca7c
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x10015980 (84)
};

class PauseAll : public Command {
public:
    PauseAll(); // 0x10028404 (56): vtable 0x1003c984
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x10014f08 (84)
};

class RestartAll : public Command {
public:
    RestartAll(); // 0x1002852c (56): vtable 0x1003c928
    TbError Execute(CAudioComponentAccess* access) const override; // unit w79d, 0x100150b4 (84)
};

// Start::Execute's two function-local statics (Mac 0x1003f354 "initialised" and 0x1003f355 "in start", both bytes, zero in the
// image): the first call clears the second and sets the first; the second is the re-entry guard. Tests reset them. (w79d)
extern bool sStartInitialised;
extern bool sStartInProgress;

}  // namespace SystemCommand
