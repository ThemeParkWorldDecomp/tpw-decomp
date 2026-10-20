# Unit v5376: std::basic_string<char> assign and change_size (src/TPMain_std_basic_string_assign.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_basic_string_assign.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_std_basic_string_assign src/TPMain_std_basic_string_assign.cpp platform/arena.cpp)
