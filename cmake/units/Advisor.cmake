set(TPW_UNIT_Advisor src/game/advisor/Advisor.cpp src/game/TPCamera.cpp src/game/advisor/TPAdvisor.cpp
    src/game/TPEmbed.cpp src/game/TPMesh.cpp src/game/rides/RideAnim.cpp src/game/LMatrix.cpp src/game/Sound.cpp
    src/game/park/ParkSound.cpp src/game/Light.cpp ${TPW_GENERATED_DIR}/advisor_lines.cpp platform/arena.cpp)
set(TPW_UNIT_Advisor_KEEP
  __Z10ADV_GetTag15AdvisorResponse   # ADV_GetTag(AdvisorResponse)
  __Z11ADV_UnPausev   # ADV_UnPause()
  __Z11LUBackSubstR7sMatrixPiPf   # LUBackSubst(sMatrix&, int*, float*)
  __Z11LoadAdvisorPcS_S_j   # LoadAdvisor(char*, char*, char*, unsigned int)
  __Z11TickAdvisorj   # TickAdvisor(unsigned int)
  __Z12DressAdvisorv   # DressAdvisor()
  __Z12InvertMatrixP7sMatrixPKS_   # InvertMatrix(sMatrix*, sMatrix const*)
  __Z13GetAnimLengthP16tag_sRideAnimSetjj   # GetAnimLength(tag_sRideAnimSet*, unsigned int, unsigned int)
  __Z14DestroyAdvisorj   # DestroyAdvisor(unsigned int)
  __Z14SND_DuckVolumei   # SND_DuckVolume(int)
  __Z15GetDeferredAnimP16tag_sRideAnimSetPjS1_j   # GetDeferredAnim(tag_sRideAnimSet*, unsigned int*, unsigned int*, unsigned int)
  __Z16SetMeshInUISpaceRP9tag_sMesh   # SetMeshInUISpace(tag_sMesh*&)
  __Z17SetAdvisorHatHandi   # SetAdvisorHatHand(int)
  __Z17SetCannedSequenceii   # SetCannedSequence(int, int)
  __Z17SetMatrixIdentityP7sMatrix   # SetMatrixIdentity(sMatrix*)
  __Z18GetAdvisorRideAnimj   # GetAdvisorRideAnim(unsigned int)
  __Z18SOUND_SetSfxStatusj   # SOUND_SetSfxStatus(unsigned int)
  __Z19ScaleLMatrixNoTransP7sMatrixP8s3DPoint   # ScaleLMatrixNoTrans(sMatrix*, s3DPoint*)
  __Z20GenerateAnimSequencei   # GenerateAnimSequence(int)
  __Z20SOUND_SetMusicStatusj   # SOUND_SetMusicStatus(unsigned int)
  __Z21DisableAdvisorDisplayj   # DisableAdvisorDisplay(unsigned int)
  __Z21FindDataNodeIndexByIDP16tag_sRideAnimSetjj   # FindDataNodeIndexByID(tag_sRideAnimSet*, unsigned int, unsigned int)
  __Z21SOUND_SetSpeechStatusj   # SOUND_SetSpeechStatus(unsigned int)
  __Z21StartAdvisorAnimationjj   # StartAdvisorAnimation(unsigned int, unsigned int)
  __Z22EnableAdvisorAccessoryjj   # EnableAdvisorAccessory(unsigned int, unsigned int)
  __Z23DisableAdvisorAccessoryjj   # DisableAdvisorAccessory(unsigned int, unsigned int)
  __Z23VectorThroughLMatrixNoWP7sMatrixP8s3DPoint   # VectorThroughLMatrixNoW(sMatrix*, s3DPoint*)
  __Z25CombineMatricesNoWDest2ndP7sMatrixS0_   # CombineMatricesNoWDest2nd(sMatrix*, sMatrix*)
  __Z30FindDataNodeIndexByDisableCodeP16tag_sRideAnimSetjj   # FindDataNodeIndexByDisableCode(tag_sRideAnimSet*, unsigned int, unsigned int)
  __Z34GetLMatrixRotationAroundZAxisYOnlyP7sMatrix   # GetLMatrixRotationAroundZAxisYOnly(sMatrix*)
  __Z36FindDataNodeIndexByIDAdvisorTempHackP16tag_sRideAnimSetjj   # FindDataNodeIndexByIDAdvisorTempHack(tag_sRideAnimSet*, unsigned int, unsigned int)
  __Z8ADV_Call15AdvisorResponse   # ADV_Call(AdvisorResponse)
  __Z8ADV_Initv   # ADV_Init()
  __Z8ADV_Killi   # ADV_Kill(int)
  __Z8ADV_Slapv   # ADV_Slap()
  __Z8ADV_Statv   # ADV_Stat()
  __Z8ADV_Tickv   # ADV_Tick()
  __Z8LUDecompR7sMatrixPiPf   # LUDecomp(sMatrix&, int*, float*)
  __Z9ADV_Closev   # ADV_Close()
  __Z9ADV_Pausev   # ADV_Pause()
  __Z9ADV_Tick2v   # ADV_Tick2()
  __ZN13sAdvisorStoreC1Ev   # sAdvisorStore::sAdvisorStore()
  __ZN6CLight11GetLinkTypeEv   # CLight::GetLinkType()
  __ZN6CLight7GetDataEv   # CLight::GetData()
  __ZN6CLight7PrepareEv   # CLight::Prepare()
  __ZN6CLightC1Ev   # CLight::CLight()
  __ZN6CLightD1Ev   # CLight::~CLight()
  _gAdvisorFile
  _gAdvisorState
  _gAdvisorStaticBuffer
  _gAdvisorStaticInUse
  _gAdvisorStore
  _gAdvisorThemeSoundBank
  _gAudioSystem
  _gCannedSequences
  _gLobbyIslandSoundCount
  _gLobbyIslandSounds
  _gLobbyThunderBank
  _gLocalParkMusicBank
  _gSoundAllocations
  _gSoundBanks
  _gSoundSettings
  _gSoundSystemOn
  _gSoundWanted
)
