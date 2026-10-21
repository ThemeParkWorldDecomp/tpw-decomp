# Unit v5263: the Sound Sprockets channel (src/sound/PlaceHolderSentence.cpp). The callees outside the unit are
# recording fakes in tests/unit_CSprocketsChannel_Callback.cpp.
set(TPW_UNIT_CSprocketsChannel_Callback src/sound/PlaceHolderSentence.cpp platform/arena.cpp)
set(TPW_UNIT_CSprocketsChannel_Callback_KEEP
  __ZN20CSprocketsChannelMac4PlayEv   # CSprocketsChannelMac::Play()
  __ZN20CSprocketsChannelMac5Set3DEh   # CSprocketsChannelMac::Set3D(unsigned char)
  __ZN20CSprocketsChannelMac7CleanupEv   # CSprocketsChannelMac::Cleanup()
  __ZN20CSprocketsChannelMac8CallbackEP10SndCommand   # CSprocketsChannelMac::Callback(SndCommand*)
  __ZN20CSprocketsChannelMacC1Ev   # CSprocketsChannelMac::CSprocketsChannelMac()
  __ZN20CSprocketsChannelMacD1Ev   # CSprocketsChannelMac::~CSprocketsChannelMac()
  __ZN20CSprocketsChannelMacdlEPv   # CSprocketsChannelMac::operator delete(void*)
)
