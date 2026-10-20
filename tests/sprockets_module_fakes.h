// The slots of CSprocketsModule that the unit libraries SoundCore and SoundManager never reach, defined as stubs. Unit p-sound
// completed include/CSprocketsModule.h (every override of the Mac's 22 functions is declared, the real ones are in
// platform/sound_module.cpp, which these libraries do not link); a unit library that fakes the module still defines the
// constructor and the four slots its own way and includes this file after them, so that the class's vtable resolves. None of
// these is called: the module in those units is a FakeModule built in place of the object.
#pragma once
#include "CSprocketsModule.h"

CSprocketsModule::~CSprocketsModule() {}
TbError CSprocketsModule::SetOptions(uint32_t) { return TbError(-77); }
TbError CSprocketsModule::Init(const TbSoundSystemInfo&) { return TbError(-77); }
TbError CSprocketsModule::StopAll() { return TbError(-77); }
TbError CSprocketsModule::PauseChannel(uint32_t) { return TbError(-77); }
TbError CSprocketsModule::RestartChannel(uint32_t) { return TbError(-77); }
TbError CSprocketsModule::PauseAll() { return TbError(-77); }
TbError CSprocketsModule::RestartAll() { return TbError(-77); }
TbError CSprocketsModule::SetSpeakerConfig(const TbSpeaker::Config&) { return TbError(-77); }
TbError CSprocketsModule::Set3DDSPFunctionality(uint32_t, uint32_t) { return TbError(-77); }
TbError CSprocketsModule::SetSpeedOfSound(float) { return TbError(-77); }
TbError CSprocketsModule::GetPosition(uint32_t, TbSoundPosition&) { return TbError(-77); }
TbError CSprocketsModule::SetListenerVelocity(const TbSoundPosition&) { return TbError(-77); }
TbError CSprocketsModule::SetListenerPosition(const TbSoundPosition&) { return TbError(-77); }
TbError CSprocketsModule::SetListenerDirection(const TbSoundPosition&, const TbSoundPosition&) { return TbError(-77); }
TbError CSprocketsModule::SetChannelInfo(uint32_t, TbSoundPipeChannel*, TbSoundSampleInfo&) { return TbError(-77); }
