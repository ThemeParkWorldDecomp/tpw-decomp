# Unit v51057: small e-mail functions (src/game/online/email.cpp). The callees outside the unit are
# recording fakes in tests/unit_AddressBook_First.cpp, which also defines the globals of other files.
set(TPW_UNIT_AddressBook_First src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_AddressBook_First_KEEP
  __ZN12email_callee8MailCbfnEiiPhPv   # email_callee::MailCbfn(int, int, unsigned char*, void*)
  __ZN6v5105710IosRdstateEPKh   # v51057::IosRdstate(unsigned char const*)
  __ZN6v5105710StringLessEPKhS1_   # v51057::StringLess(unsigned char const*, unsigned char const*)
  __ZN6v5105710StringSizeEPKh   # v51057::StringSize(unsigned char const*)
  __ZN6v5105711BasicIosTieEPKh   # v51057::BasicIosTie(unsigned char const*)
  __ZN6v5105711IosOrAssignEPjj   # v51057::IosOrAssign(unsigned int*, unsigned int)
  __ZN6v5105711IstreamDtorEPhs   # v51057::IstreamDtor(unsigned char*, short)
  __ZN6v5105711StringBeginEPKh   # v51057::StringBegin(unsigned char const*)
  __ZN6v5105712UintListCtorEPh   # v51057::UintListCtor(unsigned char*)
  __ZN6v5105712UintListDtorEPhs   # v51057::UintListDtor(unsigned char*, short)
  __ZN6v5105713IosExceptionsEPKh   # v51057::IosExceptions(unsigned char const*)
  __ZN6v5105714StringListCtorEPh   # v51057::StringListCtor(unsigned char*)
  __ZN6v5105714StringListDtorEPhs   # v51057::StringListDtor(unsigned char*, short)
  __ZN6v5105715AddressBookNextEPh   # v51057::AddressBookNext(unsigned char*)
  __ZN6v5105716AddressBookFirstEPh   # v51057::AddressBookFirst(unsigned char*)
  __ZN6v5105717IstreamSentryDtorEPhs   # v51057::IstreamSentryDtor(unsigned char*, short)
  __ZN6v5105718OutboxUnmarkToSendEPh   # v51057::OutboxUnmarkToSend(unsigned char*)
  __ZN6v5105720OutboxUnmarkToDeleteEPh   # v51057::OutboxUnmarkToDelete(unsigned char*)
  __ZN6v510576IosAndEjj   # v51057::IosAnd(unsigned int, unsigned int)
  __ZN6v510577IosGoodEPKh   # v51057::IosGood(unsigned char const*)
  __ZN6v510578IosStateEPh   # v51057::IosState(unsigned char*)
  __ZN6v510579CSockDtorEPhs   # v51057::CSockDtor(unsigned char*, short)
  _gStringListInitWord
  _gUintListInitWord
)
