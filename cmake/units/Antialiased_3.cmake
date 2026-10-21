# Unit v5940: Antialiased<CTraits<4>> and Antialiased<CTraits<3>> (src/game/ui/3DFont.cpp). The callees outside the unit are
# recording fakes in tests/unit_Antialiased_3.cpp.
set(TPW_UNIT_Antialiased_3 src/game/ui/3DFont.cpp platform/arena.cpp)
set(TPW_UNIT_Antialiased_3_KEEP
  __ZN13tpw_antialias11AntialiasedILi3EEEvRKN13tpw_intensify7CTraitsIXT_EEERK12TbRenderAreaRK6TbRectRKN3bfl4draw7CColourERK7TbPointSJ_   # void tpw_antialias::Antialiased<3>(tpw_intensify::CTraits<3> const&, TbRenderArea const&, TbRect const&, bfl::draw::CColour const&, TbPoint const&, TbPoint const&)
  __ZN13tpw_antialias11AntialiasedILi4EEEvRKN13tpw_intensify7CTraitsIXT_EEERK12TbRenderAreaRK6TbRectRKN3bfl4draw7CColourERK7TbPointSJ_   # void tpw_antialias::Antialiased<4>(tpw_intensify::CTraits<4> const&, TbRenderArea const&, TbRect const&, bfl::draw::CColour const&, TbPoint const&, TbPoint const&)
)
