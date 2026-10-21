# Unit w152 (main app): the park analyser's shop and sideshow superlatives, CParkAnalyser::GetShopWith{Best,Worst}{Value,Satisfaction} (both overloads) and
# GetSideshowWith{Best,Worst}{Value,Satisfaction} (src/game/park/analyser.cpp). The callees outside the unit are recording fakes in tests/unit_ParkShops.cpp.
set(TPW_UNIT_ParkShops src/game/park/analyser.cpp)
set(TPW_UNIT_ParkShops_KEEP
  __ZNK13CParkAnalyser20GetShopWithBestValueE15CObjectShopTypeR12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetShopWithBestValue(CObjectShopType, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser20GetShopWithBestValueER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetShopWithBestValue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser21GetShopWithWorstValueE15CObjectShopTypeR12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetShopWithWorstValue(CObjectShopType, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser21GetShopWithWorstValueER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetShopWithWorstValue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser24GetSideshowWithBestValueER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetSideshowWithBestValue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser25GetSideshowWithWorstValueER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetSideshowWithWorstValue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser27GetShopWithBestSatisfactionE15CObjectShopTypeR12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetShopWithBestSatisfaction(CObjectShopType, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser27GetShopWithBestSatisfactionER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetShopWithBestSatisfaction(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser28GetShopWithWorstSatisfactionE15CObjectShopTypeR12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetShopWithWorstSatisfaction(CObjectShopType, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser28GetShopWithWorstSatisfactionER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetShopWithWorstSatisfaction(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser31GetSideshowWithBestSatisfactionER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetSideshowWithBestSatisfaction(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser32GetSideshowWithWorstSatisfactionER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetSideshowWithWorstSatisfaction(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
)
