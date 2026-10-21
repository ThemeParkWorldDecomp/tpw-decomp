# The Bullfrog library's TbRect::Intersection (unit w45a): no callees, no fakes.
set(TPW_UNIT_TbRectIntersection src/bullfrog/LbRect.cpp)
set(TPW_UNIT_TbRectIntersection_KEEP
  __ZNK6TbRect12IntersectionERKS_   # TbRect::Intersection(TbRect const&) const
  __ZNK6TbRect8BoundingERKS_   # TbRect::Bounding(TbRect const&) const
)
