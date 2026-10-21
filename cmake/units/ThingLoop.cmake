# Unit w123a: the seven thing-loop entry points, with every outside callee faked.
set(TPW_UNIT_ThingLoop src/game/Thing.cpp src/game/advisor/Advisor.cpp src/game/park/MapDelta.cpp
    src/game/park/Weather.cpp src/game/ui/MessageCentre.cpp)
set(TPW_UNIT_ThingLoop_KEEP
  __ZN10CTagSystem4InitEv   # CTagSystem::Init()
  __ZN14CMessageCentre9SubscribeERK6CThing7MsgType   # CMessageCentre::Subscribe(CThing const&, MsgType)
  __ZN6CThing6KillMeEv   # CThing::KillMe()
  __ZN6CThing7ProcessEv   # CThing::Process()
  __ZN8CAdvisor4InitEv   # CAdvisor::Init()
  __ZN8CMapCell11InsertThingE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMapCell::InsertThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZNK8CWeather14FindNewQualityEv   # CWeather::FindNewQuality() const
)
