# Unit v5140: ccServerInfo::~ccServerInfo (src/game/online/FS_Online.cpp). free is a recording fake in
# tests/unit_ccServerInfo_Dtor_ccServerInfo.cpp.
set(TPW_UNIT_ccServerInfo_Dtor_ccServerInfo src/game/online/FS_Online.cpp platform/arena.cpp)
set(TPW_UNIT_ccServerInfo_Dtor_ccServerInfo_KEEP
  __ZN12ccServerInfoD1Ev   # ccServerInfo::~ccServerInfo()
)
