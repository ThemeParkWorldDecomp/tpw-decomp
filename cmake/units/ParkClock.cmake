# The park clock's leaves (unit w125, bullfrog): TbTimeStamp::operator+, operator-, operator> and GetTime. NS_MacDoze::FileTimeToSystemTime is a fake
# in the unit file; the stamps are plain 64-bit values on both sides.
set(TPW_UNIT_ParkClock src/TbTimeStamp_ops.cpp)
