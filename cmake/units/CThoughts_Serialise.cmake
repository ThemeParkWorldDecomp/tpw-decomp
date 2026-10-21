# Unit v5888: CThoughts (src/game/people/Thoughts.cpp). The callees outside the unit are recording fakes in
# tests/unit_CThoughts_Serialise.cpp, which also defines the globals of other files.
set(TPW_UNIT_CThoughts_Serialise src/game/people/Thoughts.cpp platform/arena.cpp)
set(TPW_UNIT_CThoughts_Serialise_KEEP
  __ZN9CThoughts11RecordEventENS_9EventTypeE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CThoughts::RecordEvent(CThoughts::EventType, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN9CThoughts17ShowThoughtBubbleENS_11ThoughtTypeEb   # CThoughts::ShowThoughtBubble(CThoughts::ThoughtType, bool)
  __ZN9CThoughts19RemoveThoughtBubbleEv   # CThoughts::RemoveThoughtBubble()
  __ZN9CThoughts21ProcessThoughtBubblesEv   # CThoughts::ProcessThoughtBubbles()
  __ZN9CThoughts9SerialiseEPviij   # CThoughts::Serialise(void*, int, int, unsigned int)
  __ZN9CThoughtsC1Ev   # CThoughts::CThoughts()
  __ZNK9CThoughts14GetRecentEventEj   # CThoughts::GetRecentEvent(unsigned int) const
  _gEventHistCtorVec
  _gThoughtScripts
)
