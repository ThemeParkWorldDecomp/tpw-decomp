# Unit w202: the ride's admittance and queue length (src/game/Object.cpp). The callees outside the unit are fakes in tests/unit_ObjectAdmit.cpp (the assert, the
# trace, the ride script's variables and tour, the ride record lookup, the thing table's guests and the five CPerson members the ride calls, the queue count and
# removal, the coaster test, SetState, the hoardings, the first person camera and the two camera mesh finders); nothing else is linked.
set(TPW_UNIT_ObjectAdmit src/game/Object.cpp)
set(TPW_UNIT_ObjectAdmit_KEEP
  __ZN7CObject11AdmitPersonE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CObject::AdmitPerson(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN7CObject11LetPeopleOnEv   # CObject::LetPeopleOn()
  __ZN7CObject12LetPeopleOffEv   # CObject::LetPeopleOff()
  __ZN7CObject12ReadyToAdmitE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CObject::ReadyToAdmit(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN7CObject17FinishedAdmittingEv   # CObject::FinishedAdmitting()
  __ZN7CObject23ProcessStateFunctioningEv   # CObject::ProcessStateFunctioning()
  __ZN7CObject26PersonLeftDuringAdmittanceE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CObject::PersonLeftDuringAdmittance(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZNK7CObject12QueueTooLongEv   # CObject::QueueTooLong() const
  __ZNK7CObject13GoFirstPersonEi   # CObject::GoFirstPerson(int) const
  __ZNK7CObject17HasFreeQueueSpaceEv   # CObject::HasFreeQueueSpace() const
  __ZNK7CObject19GetIdealQueueLengthEv   # CObject::GetIdealQueueLength() const
  _gObjectQueueSpeedRatio
)
