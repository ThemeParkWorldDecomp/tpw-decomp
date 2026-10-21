# The park clock's leaves (unit w125, bullfrog): TbTimeStamp::operator+, operator-, operator> and GetTime. NS_MacDoze::FileTimeToSystemTime is a fake
# in the unit file; the stamps are plain 64-bit values on both sides.
set(TPW_UNIT_ParkClock src/bullfrog/TbTimeStamp.cpp)
set(TPW_UNIT_ParkClock_KEEP
  __ZNK11TbTimeStamp7GetTimeEPiS0_S0_S0_S0_S0_S0_S0_   # TbTimeStamp::GetTime(int*, int*, int*, int*, int*, int*, int*, int*) const
  __ZNK11TbTimeStampgtERKS_   # TbTimeStamp::operator>(TbTimeStamp const&) const
  __ZNK11TbTimeStampmiERKS_   # TbTimeStamp::operator-(TbTimeStamp const&) const
  __ZNK11TbTimeStampplERK10TbTimeDiff   # TbTimeStamp::operator+(TbTimeDiff const&) const
)
