# Unit w112b: the dynamic cell meshes (CDynamicMesh, DMCreateMesh, DMEnableMeshRender, CorrectDTexturePaths, LLCC_SystemGetMode). The arena is needed for
# the Ptr32 fields of the mesh block (as in RideDestroy and RideAnimInterp). Every callee outside the file is a fake in tests/unit_DynamicMesh.cpp.
set(TPW_UNIT_DynamicMesh src/DynamicMesh.cpp platform/arena.cpp)
