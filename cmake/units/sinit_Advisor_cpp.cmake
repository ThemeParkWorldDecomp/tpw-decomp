# Unit v51069: Advisor.cpp's static initialiser and save templates, small leaves of the bump rides, camera and coasters
# (src/game/TPCamera.cpp, src/game/advisor/Advisor.cpp, src/game/rides/BumperRide.cpp, src/game/rides/CoasterQueue.cpp, src/game/rides/CoasterTrain.cpp, src/game/rides/TPCoaster.cpp). The callees outside the unit are recording fakes in tests/unit_sinit_Advisor_cpp.cpp,
# which also defines the globals of other files.
set(TPW_UNIT_sinit_Advisor_cpp src/game/TPCamera.cpp src/game/advisor/Advisor.cpp src/game/rides/BumperRide.cpp
    src/game/rides/CoasterQueue.cpp src/game/rides/CoasterTrain.cpp src/game/rides/TPCoaster.cpp platform/arena.cpp)
set(TPW_UNIT_sinit_Advisor_cpp_KEEP
  __Z12BMPFile_ReadPvS_jPj   # BMPFile_Read(void*, void*, unsigned int, unsigned int*)
  __Z12DestroyQueueP16sWraparoundQueue   # DestroyQueue(sWraparoundQueue*)
  __Z13GetFinishLineP11tagBUMPRIDE   # GetFinishLine(tagBUMPRIDE*)
  __Z13GetRideFromIDRj   # GetRideFromID(unsigned int&)
  __Z15SetBuoySequenceP11tagBUMPRIDE   # SetBuoySequence(tagBUMPRIDE*)
  __Z16__arraydtor_3112v   # __arraydtor_3112()
  __Z19GetCoasterPeepStatsjPfS_   # GetCoasterPeepStats(unsigned int, float*, float*)
  __Z19IsEndOfSampleBufferP20tag_sCoasterInstancef   # IsEndOfSampleBuffer(tag_sCoasterInstance*, float)
  __Z19ResetCameraPositionv   # ResetCameraPosition()
  __Z19__sinit_Advisor_cppv   # __sinit_Advisor_cpp()
  __Z20GetCoasterInstanceIDi   # GetCoasterInstanceID(int)
  __Z20GetFirstPersonRideIDv   # GetFirstPersonRideID()
  __Z21FreeColObjectsForRideP11tagBUMPRIDE   # FreeColObjectsForRide(tagBUMPRIDE*)
  __Z21__sinit_TPCoaster_cppv   # __sinit_TPCoaster_cpp()
  __Z22GetDataNodeWorldMatrixRP9tag_sMeshRj   # GetDataNodeWorldMatrix(tag_sMesh*&, unsigned int&)
  __Z23GetNthCoasterTrackStartjPPv   # GetNthCoasterTrackStart(unsigned int, void**)
  __Z26DoImmediatePhysicsFeedbackP20tag_sCoasterInstance   # DoImmediatePhysicsFeedback(tag_sCoasterInstance*)
  __Z8FindBuoyP11tagBUMPRIDEii   # FindBuoy(tagBUMPRIDE*, int, int)
  _gAdvisorMsgStringNode
  _gAdvisorMsgStringStorage
  _gAdvisorMsgText
  _gAdvisorStoreDtorVector
  _gTPCoasterWord7314
  _gTPCoasterWord7318
  TRAINS_SWAP_FILE_WORDS@game/rides/TPCoaster.cpp   # the #define TRAINS_SWAP_FILE_WORDS of src/game/rides/TPCoaster.cpp (a guard macro the regroup brought into it)
)
