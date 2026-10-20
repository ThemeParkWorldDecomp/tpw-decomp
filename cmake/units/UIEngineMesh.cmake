# The engine mesh draw objects' instancing and changes, the gradients' Change and the UI matrix helpers (unit w42). The matrix leaves
# (SetMatrixIdentity, VectorThroughLMatrixNoW) and the rectangles are real; InstanceMesh, the engine mesh's local matrix, the draw object
# base class and the constructors are fakes in the unit file.
set(TPW_UNIT_UIEngineMesh src/ui_DrawObjectEngineMesh.cpp src/ui_matrix.cpp src/rsse_matrix.cpp src/UI_Base.cpp platform/arena.cpp)
