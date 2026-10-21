# Unit v5962: CountDataNodesByType, FindNthDataNodeIndex, EnableRideHead, bthread_cond (ctor, dtor, wait), CalculateOrientFromTrackMorph
# (src/game/TPEmbed.cpp, src/game/online/bthread.cpp). The callees outside the unit are recording fakes in tests/unit_CountDataNodesByType.cpp.
set(TPW_UNIT_CountDataNodesByType src/game/TPEmbed.cpp src/game/online/bthread.cpp platform/arena.cpp
    tests/standins/CountDataNodesByType.cpp)
set(TPW_UNIT_CountDataNodesByType_KEEP
  __Z14EnableRideHeadP9tag_sMeshjj   # EnableRideHead(tag_sMesh*, unsigned int, unsigned int)
  __Z20CountDataNodesByTypeP16tag_sRideAnimSetj   # CountDataNodesByType(tag_sRideAnimSet*, unsigned int)
  __Z20FindNthDataNodeIndexP16tag_sRideAnimSetjj   # FindNthDataNodeIndex(tag_sRideAnimSet*, unsigned int, unsigned int)
  __Z29CalculateOrientFromTrackMorphP8s3DPointS0_S0_P20sTrackMorphingParentP7sMatrix   # CalculateOrientFromTrackMorph(s3DPoint*, s3DPoint*, s3DPoint*, sTrackMorphingParent*, sMatrix*)
  __ZN10NS_MacDoze11CreateEventEPviiPKc   # NS_MacDoze::CreateEvent(void*, int, int, char const*)
  __ZN10NS_MacDoze19WaitForSingleObjectEPNS_14InternalHandleEj   # NS_MacDoze::WaitForSingleObject(NS_MacDoze::InternalHandle*, unsigned int)
  __ZN12bthread_cond4waitEi   # bthread_cond::wait(int)
  __ZN12bthread_cond7DestroyEPS_s   # bthread_cond::Destroy(bthread_cond*, short)
  __ZN12bthread_condC1Ev   # bthread_cond::bthread_cond()
  __ZN14bthread_callee14OperatorDeleteEPv   # bthread_callee::OperatorDelete(void*)
  _gRideHeadSpawnBlock
)
