# Unit v5376: std::basic_string<char> assign and change_size (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_basic_string_assign.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_std_basic_string_assign src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_string_assign_KEEP
  __ZN15msl_char_string11change_sizeEP13MslCharStringjb   # msl_char_string::change_size(MslCharString*, unsigned int, bool)
  __ZN15msl_char_string6assignEP13MslCharStringPKS0_jj   # msl_char_string::assign(MslCharString*, MslCharString const*, unsigned int, unsigned int)
)
