# Unit w189: CStaff part 1 (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, UpdateTurn, SetState, ModelState, UpdateMonthly, the four
# PayGrade / StaffState serialise instances, CMsgStaffRoomEvent's destructor and GetType, __sinit_staff_cpp; src/game/people/staff.cpp). Every callee outside them (the
# CPeep members, the thoughts, the delayed script, the other CStaff members, the world, the bank account, the park analyser, the message centre, the mouse,
# the other serialise instances, the trace and the assertion) is a recording fake in tests/unit_Staff.cpp, which also defines the globals of other files.
set(TPW_UNIT_Staff src/game/people/staff.cpp platform/arena.cpp)
set(TPW_UNIT_Staff_KEEP
  __Z7LOADVARIN6CStaff10StaffStateEEiPvPT_PKci   # int LOADVAR<CStaff::StaffState>(void*, CStaff::StaffState*, char const*, int)
  __Z7LOADVARIN6CStaff8PayGradeEEiPvPT_PKci   # int LOADVAR<CStaff::PayGrade>(void*, CStaff::PayGrade*, char const*, int)
  __Z7SAVEVARIN6CStaff10StaffStateEEiPvPT_PKci   # int SAVEVAR<CStaff::StaffState>(void*, CStaff::StaffState*, char const*, int)
  __Z7SAVEVARIN6CStaff8PayGradeEEiPvPT_PKci   # int SAVEVAR<CStaff::PayGrade>(void*, CStaff::PayGrade*, char const*, int)
  __ZN18CMsgStaffRoomEventD1Ev   # CMsgStaffRoomEvent::~CMsgStaffRoomEvent()
  __ZN6CStaff10ModelStateEv   # CStaff::ModelState()
  __ZN6CStaff10UpdateTurnEv   # CStaff::UpdateTurn()
  __ZN6CStaff12FinalDestroyEv   # CStaff::FinalDestroy()
  __ZN6CStaff13UpdateMonthlyEv   # CStaff::UpdateMonthly()
  __ZN6CStaff14ReceiveMessageERK4CMsg   # CStaff::ReceiveMessage(CMsg const&)
  __ZN6CStaff4InitEv   # CStaff::Init()
  __ZN6CStaff8SetStateENS_10StaffStateE   # CStaff::SetState(CStaff::StaffState)
  __ZN6CStaff9SerialiseEPviij   # CStaff::Serialise(void*, int, int, unsigned int)
  __ZN6CStaffC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEN6CThing10ThingModelE   # CStaff::CStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, CThing::ThingModel)
  __ZN6CStaffC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEN6CThing10ThingModelENS_8PayGradeEh   # CStaff::CStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, CThing::ThingModel, CStaff::PayGrade, unsigned char)
  __ZNK18CMsgStaffRoomEvent7GetTypeEv   # CMsgStaffRoomEvent::GetType() const
  _gStaffSinitHigh
  _gStaffSinitLow
)
