# Unit v5706: BMP_Distance, PossitionSound1, SingleSoundAtBump, MineToSystemCords, NormaliseVector (src/game/rides/BumperRide.cpp).
# The sound callees are recording fakes in tests/unit_BMP_Distance.cpp, which also defines gSoundBanks.
set(TPW_UNIT_BMP_Distance src/game/rides/BumperRide.cpp platform/arena.cpp)
set(TPW_UNIT_BMP_Distance_KEEP
  __Z12BMP_Distanceii   # BMP_Distance(int, int)
  __Z15NormaliseVectorP8s3DPoint   # NormaliseVector(s3DPoint*)
  __Z15PossitionSound1P7tagBUMP   # PossitionSound1(tagBUMP*)
  __Z17MineToSystemCordsiiPN10NS_MacDoze8tagPOINTE   # MineToSystemCords(int, int, NS_MacDoze::tagPOINT*)
  __Z17SingleSoundAtBumpP7tagBUMPi   # SingleSoundAtBump(tagBUMP*, int)
)
