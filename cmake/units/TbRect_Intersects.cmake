# Units v5750 and v5L16: TbRect::Intersects and TbRect::Normalise (src/bullfrog/LbRect.cpp).
set(TPW_UNIT_TbRect_Intersects src/bullfrog/LbRect.cpp platform/arena.cpp)
set(TPW_UNIT_TbRect_Intersects_KEEP
  __ZN6TbRect9NormaliseEv   # TbRect::Normalise()
  __ZNK6TbRect10IntersectsERKS_   # TbRect::Intersects(TbRect const&) const
)
