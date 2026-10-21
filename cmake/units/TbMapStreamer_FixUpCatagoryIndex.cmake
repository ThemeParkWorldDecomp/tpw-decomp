# Unit v51113: small sound-library functions (src/sound/CSprocketsModule.cpp, src/sound/TbMapStreamer.cpp, src/sound/TbSoundCore.cpp, src/sound/TbSoundSampleStore.cpp, src/sound/TpBody.cpp). The callees outside the unit are
# recording fakes in tests/unit_TbMapStreamer_FixUpCatagoryIndex.cpp.
set(TPW_UNIT_TbMapStreamer_FixUpCatagoryIndex src/sound/CSprocketsModule.cpp src/sound/TbMapStreamer.cpp
    src/sound/TbSoundCore.cpp src/sound/TbSoundSampleStore.cpp src/sound/TpBody.cpp platform/arena.cpp)
set(TPW_UNIT_TbMapStreamer_FixUpCatagoryIndex_KEEP
  __ZN13SamsUtilities12UCommandPlayC1Ej   # SamsUtilities::UCommandPlay::UCommandPlay(unsigned int)
  __ZN13TbMapStreamer18FixUpCatagoryIndexEj   # TbMapStreamer::FixUpCatagoryIndex(unsigned int)
  __ZN13TbMapStreamer23UnloadCatgoryFixupTableEv   # TbMapStreamer::UnloadCatgoryFixupTable()
  __ZN16CSprocketsModule10RestartAllEv   # CSprocketsModule::RestartAll()
  __ZN16CSprocketsModule14RestartChannelEj   # CSprocketsModule::RestartChannel(unsigned int)
  __ZN18TbMapStreamerFixup10StringTextEPv   # TbMapStreamerFixup::StringText(void*)
  __ZN21_CS_TbSoundSamplePipe8DestructEPS_s   # _CS_TbSoundSamplePipe::Destruct(_CS_TbSoundSamplePipe*, short)
  __ZN21_CS__TbSoundCodecLoop8DestructEPS_s   # _CS__TbSoundCodecLoop::Destruct(_CS__TbSoundCodecLoop*, short)
  __ZN5tbstd11sqrtOfFloatEf   # tbstd::sqrtOfFloat(float)
  __ZN5tbstd5sqrtfEf   # tbstd::sqrtf(float)
  __ZN6TpBody4InitEPKc   # TpBody::Init(char const*)
  __ZN6TpBody4TickEv   # TpBody::Tick()
  __ZN6TpBody5CloseEv   # TpBody::Close()
  __ZN6TpBody8DestructEPS_s   # TpBody::Destruct(TpBody*, short)
  __ZNK6TpBody17SetCatagoryReverbEj   # TpBody::SetCatagoryReverb(unsigned int) const
  __ZNK6TpBody23DeregisterCatagoryBanksEj   # TpBody::DeregisterCatagoryBanks(unsigned int) const
  __ZNK6TpBody8SetFadesEj   # TpBody::SetFades(unsigned int) const
)
