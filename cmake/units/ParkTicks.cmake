# The park loop's small ticks (unit w133): TickLevelLights, TickFlashObjects, TickRideHighlights, SetBaseBlendFlags,
# LLCC_DrawInitialHoarding, CIntrface::DoPointedThingComputations, FLY_SetFlyZone, CActionRecorder::DoTick and
# ResetActionRecording. Light.cpp is the light the sun's tick writes; the arena holds the meshes the test builds.
set(TPW_UNIT_ParkTicks src/game/TPLevel.cpp src/game/rides/RidePreview.cpp src/game/park/SkirtDeform.cpp
    src/game/park/LLCellSystem.cpp src/game/ui/intrface.cpp src/game/rides/FlyingRide.cpp src/game/ActionRecorder.cpp
    src/game/Light.cpp platform/arena.cpp src/bullfrog/FileSystem.cpp platform/msl.cpp)
set(TPW_UNIT_ParkTicks_KEEP
  __Z14FLY_SetFlyZonev   # FLY_SetFlyZone()
  __Z15TickLevelLightsi   # TickLevelLights(int)
  __Z16TickFlashObjectsv   # TickFlashObjects()
  __Z17SetBaseBlendFlagsP9tag_sMeshP9tag_sNodei   # SetBaseBlendFlags(tag_sMesh*, tag_sNode*, int)
  __Z18TickRideHighlightsv   # TickRideHighlights()
  __Z24LLCC_DrawInitialHoardingv   # LLCC_DrawInitialHoarding()
  __ZN15CActionRecorder20ResetActionRecordingEv   # CActionRecorder::ResetActionRecording()
  __ZN15CActionRecorder6DoTickEv   # CActionRecorder::DoTick()
  __ZN6CLight11GetLinkTypeEv   # CLight::GetLinkType()
  __ZN6CLight7GetDataEv   # CLight::GetData()
  __ZN6CLight7PrepareEv   # CLight::Prepare()
  __ZN6CLightC1Ev   # CLight::CLight()
  __ZN6CLightD1Ev   # CLight::~CLight()
  __ZN9CIntrface26DoPointedThingComputationsEv   # CIntrface::DoPointedThingComputations()
  _gFlashObjects
  _gFlashPhase
  _gFlyZoneA
  _gFlyZoneB
  _gFlyZoneC
  _gFlyZoneD
  _gGlobalLightScale
  _gHoardingBottom
  _gHoardingLeft
  _gHoardingRight
  _gHoardingTop
  _gLLCCE0CC
  __ZN22TbStaticStringTemplateIcE8ResizeToEj   # template void TbStaticStringTemplate<char>::ResizeTo(uint32_t) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEC1EPcji   # template TbStaticStringTemplate<char>::TbStaticStringTemplate(char*, uint32_t, int32_t) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcED1Ev   # template TbStaticStringTemplate<char>::~TbStaticStringTemplate() (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEaSEPKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const char*) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEaSERK12TbStringBaseIcE   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const TbStringBase<char>&) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEaSERKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const char&) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEpLEPKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const char*) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEpLERK12TbStringBaseIcE   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const TbStringBase<char>&) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEpLERKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const char&) (explicit instantiation)
  __ZTS12TbStringBaseIDsE   # template class TbStringBase<char16_t> (explicit instantiation)
  __ZTS12TbStringBaseIcE   # template class TbStringBase<char> (explicit instantiation)
  __ZTS23TbDynamicStringTemplateIDsE   # template class TbDynamicStringTemplate<char16_t> (explicit instantiation)
  __ZTS23TbDynamicStringTemplateIcE   # template class TbDynamicStringTemplate<char> (explicit instantiation)
  tmpl_10f1a43abff8@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbString.cpp had before the regroup)
  tmpl_12b48e599676@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_13aaac678a90@bullfrog/FileSystem.cpp   # ~TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_156c9453e350@bullfrog/FileSystem.cpp   # ResizeTo (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_177fef366926@bullfrog/FileSystem.cpp   # SBCstrcpy (a template definition src/TbString.cpp had before the regroup)
  tmpl_1b2c80ece410@bullfrog/FileSystem.cpp   # FindOneOf (a template definition src/TbString.cpp had before the regroup)
  tmpl_1eedb09deef3@bullfrog/FileSystem.cpp   # SBCstrncat (a template definition src/TbString.cpp had before the regroup)
  tmpl_1fb948d1a8d7@bullfrog/FileSystem.cpp   # operator== (a template definition src/TbString.cpp had before the regroup)
  tmpl_31520eb2cde0@bullfrog/FileSystem.cpp   # operator< (a template definition src/TbString.cpp had before the regroup)
  tmpl_3b898e0ec42d@bullfrog/FileSystem.cpp   # Length (a template definition src/TbString.cpp had before the regroup)
  tmpl_3f76b5cf6898@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
  tmpl_4209e9eb338a@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_444e69a72b25@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_47746be482c6@bullfrog/FileSystem.cpp   # Right (a template definition src/TbString.cpp had before the regroup)
  tmpl_5072774eb760@bullfrog/FileSystem.cpp   # Find (a template definition src/TbString.cpp had before the regroup)
  tmpl_56b8c8679dbb@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
  tmpl_58045e5a0a73@bullfrog/FileSystem.cpp   # AllocBuffer (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_5e6789bb3ff3@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_62d628fca008@bullfrog/FileSystem.cpp   # FindReverseOneOf (a template definition src/TbString.cpp had before the regroup)
  tmpl_67728c1f6bc2@bullfrog/FileSystem.cpp   # ResizeTo (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_6fee3b71ae69@bullfrog/FileSystem.cpp   # LockBuffer (a template definition src/TbString.cpp had before the regroup)
  tmpl_7a81918ce0ed@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_7d7b5671d9c8@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbString.cpp had before the regroup)
  tmpl_7f7c954a7476@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_8078b9a106a6@bullfrog/FileSystem.cpp   # Find (a template definition src/TbString.cpp had before the regroup)
  tmpl_89c56dab9bda@bullfrog/FileSystem.cpp   # Left (a template definition src/TbString.cpp had before the regroup)
  tmpl_90c753af4ad8@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_923c787ed96b@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbString.cpp had before the regroup)
  tmpl_9329dea9cdff@bullfrog/FileSystem.cpp   # operator== (a template definition src/TbString.cpp had before the regroup)
  tmpl_966b985e3b04@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_a2ea62013ee7@bullfrog/FileSystem.cpp   # SBCstrlen (a template definition src/TbString.cpp had before the regroup)
  tmpl_a65764511fd4@bullfrog/FileSystem.cpp   # ReleaseBuffer (a template definition src/TbString.cpp had before the regroup)
  tmpl_aa572ba6c8bb@bullfrog/FileSystem.cpp   # ~TbStaticStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_b04e950cbb16@bullfrog/FileSystem.cpp   # FindReverse (a template definition src/TbString.cpp had before the regroup)
  tmpl_be6e319ad7a7@bullfrog/FileSystem.cpp   # TbStaticStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_bf7094367fc1@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
  tmpl_c2f8bf00de94@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_c4b2c4a972d6@bullfrog/FileSystem.cpp   # Mid (a template definition src/TbString.cpp had before the regroup)
  tmpl_cd6077897b68@bullfrog/FileSystem.cpp   # operator!= (a template definition src/TbString.cpp had before the regroup)
  tmpl_d3aef466ce77@bullfrog/FileSystem.cpp   # SBCstrcat (a template definition src/TbString.cpp had before the regroup)
  tmpl_d3d969d3322f@bullfrog/FileSystem.cpp   # SBCstrncpy (a template definition src/TbString.cpp had before the regroup)
  tmpl_d4a4fb92aa5f@bullfrog/FileSystem.cpp   # Mid (a template definition src/TbString.cpp had before the regroup)
  tmpl_d809f9651e41@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_feba63a511e0@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
)
