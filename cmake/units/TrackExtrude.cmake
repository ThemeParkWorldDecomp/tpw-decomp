# Unit w213: the coaster track's mesh (ExtrudeTrackSection, ExtrudeCrossSectionVertices, GetNewTrackMeshSet, SkinCrossSectionFaces,
# LinkFEStoreToFETextureCont, CopyExtrudedFaceData and InterpolateCoasterSpline; src/TPConfig_extrude.cpp). The spline functions, the mesh
# calls and the globals of other files are fakes or definitions in tests/unit_TrackExtrude.cpp, which also defines gTPGlobals (the engine's
# sine table). The strings (src/TbString.cpp, src/TbStringBuffer.cpp, platform/msl.cpp) are there for gTPGlobals' default names, never read.
set(TPW_UNIT_TrackExtrude src/TPConfig_extrude.cpp platform/arena.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
