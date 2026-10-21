# Unit v51072: CActionRecorder::ProcessAction (count, text) and the stand-ins around it (src/game/ActionRecorder.cpp, src/game/ExitErr.cpp, src/game/InGame.cpp, src/game/keyboard.cpp, src/game/ui/ui_visitorStatus.cpp).
# The callees outside the unit are recording fakes in tests/unit_CActionRecorder_ProcessAction_3.cpp, which also defines the globals of other files.
set(TPW_UNIT_CActionRecorder_ProcessAction_3 src/game/ActionRecorder.cpp src/game/ExitErr.cpp src/game/InGame.cpp
    src/game/keyboard.cpp src/game/ui/ui_visitorStatus.cpp platform/arena.cpp)
set(TPW_UNIT_CActionRecorder_ProcessAction_3_KEEP
  __Z11LoadPtclLibv   # LoadPtclLib()
  __Z11show_scriptv   # show_script()
  __Z14save_recordingv   # save_recording()
  __Z15CMemFileDestroyPvs   # CMemFileDestroy(void*, short)
  __Z17WideStringDestroyPvs   # WideStringDestroy(void*, short)
  __Z18ThingIdListDestroyPvs   # ThingIdListDestroy(void*, short)
  __Z18__sinit_InGame_cppv   # __sinit_InGame_cpp()
  __Z18cheat_cleanup_parkv   # cheat_cleanup_park()
  __Z18recreate_recordingv   # recreate_recording()
  __Z19ThingArrayEntryInitPj   # ThingArrayEntryInit(unsigned int*)
  __Z19__sinit_ExitErr_cppv   # __sinit_ExitErr_cpp()
  __Z28recreate_recording_real_timev   # recreate_recording_real_time()
  __Z31recreate_recording_medium_speedv   # recreate_recording_medium_speed()
  __Z5pausev   # pause()
  __ZN13v51072_callee14OperatorDeleteEPv   # v51072_callee::OperatorDelete(void*)
  __ZN13v51072_callee20ThingIdListRemoveAllEPv   # v51072_callee::ThingIdListRemoveAll(void*)
  __ZN15CActionRecorder13ProcessActionEihPDs   # CActionRecorder::ProcessAction(int, unsigned char, char16_t*)
  __ZN15CActionRecorder19SaveRecordingToDiskEv   # CActionRecorder::SaveRecordingToDisk()
  __ZN15CActionRecorder22GetNextCameraFileEntryEv   # CActionRecorder::GetNextCameraFileEntry()
  __ZN15CActionRecorder22GetNextTimingFileEntryEv   # CActionRecorder::GetNextTimingFileEntry()
  __ZN18VisitorStatusPanel12DeInitWindowEv   # VisitorStatusPanel::DeInitWindow()
  __ZN18VisitorStatusPanel14InitViewWindowEv   # VisitorStatusPanel::InitViewWindow()
  __ZN18VisitorStatusPanel16DeInitViewWindowEv   # VisitorStatusPanel::DeInitViewWindow()
  __ZN18VisitorStatusPanel18DeInitStatusWindowEv   # VisitorStatusPanel::DeInitStatusWindow()
  __ZN18VisitorStatusPanel6LocateEv   # VisitorStatusPanel::Locate()
  __ZN18VisitorStatusPanel6RemoveEv   # VisitorStatusPanel::Remove()
  __ZN18VisitorStatusPanel7KidGoneEP6CThing   # VisitorStatusPanel::KidGone(CThing*)
  __ZNK11CWorldSavvy6GetMapEv   # CWorldSavvy::GetMap() const
  _gInGameFontDtor
  _gInGameFontNode
  __ZN7TbTListI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtES2_E9RemoveAllEv@game/ui/ui_visitorStatus.cpp   # TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>::RemoveAll() (an explicit specialization of src/game/ui/ui_visitorStatus.cpp, declared above its callers since the regroup)
)
