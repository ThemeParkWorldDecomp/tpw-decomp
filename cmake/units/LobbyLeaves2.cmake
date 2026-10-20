# The lobby's small leaves (unit w58): the face normals (RecalculateInvalid*FaceNormals), the matrix rotations by axis order, TbRect::Bounding
# and the end of the interface (DeInitGeneralUI, UIInterface::DeInitGeneral, which sit in ui_main_meshes.cpp with the rest of that file, so the
# sources of the UIStart unit are linked as well and the unit file fakes what they call). The cursor table, the heaps and the UI system are fakes.
set(TPW_UNIT_LobbyLeaves2 src/rsse_normals.cpp src/rsse_rotate.cpp src/bfl_tbrect.cpp src/InterfaceFont.cpp src/UI_Base.cpp src/ui_DrawObject.cpp
    src/ui_DrawObject_find.cpp src/ui_main_meshes.cpp src/TPAdvisor.cpp src/TPGlobals.cpp src/TPGlobals_Dtor_TPGlobals.cpp platform/arena.cpp)
