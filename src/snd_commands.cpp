// The system command constructors of the sound library (unit w79b, sound symbols shared): each stores its vptr (the Mac
// vtable addresses are in include/snd_commands.h) and at most two words. The Execute bodies are unit w79d's.
#include "snd_commands.h"

namespace SystemCommand {

Command::Command() {}

HandleCommand::HandleCommand(void* handle) : Command(), mHandle(handle) {}

SetSystemInfo::SetSystemInfo(TbSoundSystemInfo& info) : Command(), mInfo(&info) {}

SetSampleInfo::SetSampleInfo(void* handle, TbSoundSampleInfo& info) : HandleCommand(handle), mInfo(&info) {}

Stop::Stop(void* handle) : HandleCommand(handle) {}

Pause::Pause(void* handle) : HandleCommand(handle) {}

Restart::Restart(void* handle) : HandleCommand(handle) {}

Start::Start(void* handle) : HandleCommand(handle) {}

UpdateAllVolumes::UpdateAllVolumes() : Command() {}

Process::Process() : Command() {}

StopAll::StopAll() : Command() {}

PauseAll::PauseAll() : Command() {}

RestartAll::RestartAll() : Command() {}

}  // namespace SystemCommand
