# Unit v5260: TbIResourceIndex::ReleaseResource and ReleaseItem (src/ltms/TbIResourceIndex.cpp).
set(TPW_UNIT_TbIResourceIndex_ReleaseResource src/ltms/TbIResourceIndex.cpp platform/arena.cpp)
set(TPW_UNIT_TbIResourceIndex_ReleaseResource_KEEP
  __ZN16TbIResourceIndex11ReleaseItemEPNS_17resourceIndexItemE   # TbIResourceIndex::ReleaseItem(TbIResourceIndex::resourceIndexItem*)
  __ZN16TbIResourceIndex15ReleaseResourceEPv   # TbIResourceIndex::ReleaseResource(void*)
)
