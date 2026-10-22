cmake_minimum_required(VERSION 3.20)
project(tpw LANGUAGES C CXX)   # C for the vendored libraries (third_party/)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter -ffp-contract=off -fwrapv)

# The bullfrog library's zlib unpacker (src/bullfrog/LbZLib.cpp) inflates with zlib, built from source here
# (third_party/zlib, static); no copy on the host is ever looked for.
add_subdirectory(third_party/zlib)

# Four data tables of the original program (the advisor's response, message and balance description tables and the save
# file header's legal texts) are not in this repository: devtools/gen_game_tables.py reads them at configure time from the
# player's own "SimTheme Park" application (its data fork, a PEF file) into the build folder. Any full Mac build will do:
# the CD's own 2.0.1, the 2.04b3 patch's, or one of the 2.1 releases (the tables are the same in all of them).
# -DTPW_GAME_PROGRAM=<path> or the TPW_GAME_PROGRAM environment variable names it; the default is the CD's program as
# devtools/import_originals.py writes it, next to this repository.
if(NOT TPW_GAME_PROGRAM)
  if(DEFINED ENV{TPW_GAME_PROGRAM})
    set(TPW_GAME_PROGRAM "$ENV{TPW_GAME_PROGRAM}")
  else()
    set(TPW_GAME_PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/../extracted/disc-hfs/Theme Park Data/SimTheme Park")
  endif()
endif()
set(TPW_GAME_PROGRAM "${TPW_GAME_PROGRAM}" CACHE FILEPATH "A full Mac SimTheme Park application (its data fork) the data tables are read from")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(TPW_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
execute_process(COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/devtools/gen_game_tables.py
                        --program ${TPW_GAME_PROGRAM} --out ${TPW_GENERATED_DIR}
                RESULT_VARIABLE TPW_GENERATED_RESULT)
if(NOT TPW_GENERATED_RESULT EQUAL 0)
  message(FATAL_ERROR "The game's data tables could not be generated: set TPW_GAME_PROGRAM to a full Mac \"SimTheme Park\" application (the CD's own one is fine)")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             ${CMAKE_CURRENT_SOURCE_DIR}/devtools/gen_game_tables.py ${TPW_GAME_PROGRAM}
             ${CMAKE_CURRENT_SOURCE_DIR}/include/AdvisorScoring.h)   # the scorer sources are named by the header
include_directories(${TPW_GENERATED_DIR})   # SaveLegalText.inc
set(TPW_GENERATED_SOURCES ${TPW_GENERATED_DIR}/advisor_lines.cpp ${TPW_GENERATED_DIR}/AdvisorScoring_table.cpp
    ${TPW_GENERATED_DIR}/AdvisorBalance_table.cpp ${TPW_GENERATED_DIR}/AdvisorScorerSources.cpp)

file(GLOB_RECURSE TPW_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(APPEND TPW_SOURCES ${TPW_GENERATED_SOURCES})
add_library(tpw STATIC ${TPW_SOURCES})
target_include_directories(tpw PUBLIC include)
target_link_libraries(tpw PUBLIC ZLIB::ZLIB)
# Copies of src files with only some of their definitions, for the test libraries (cmake/slicer.cmake).
include(cmake/slicer.cmake)
# The platform layer's sound module (platform/sound_module.cpp) defines these CSprocketsModule methods for the launcher,
# so the game library builds src/sound/CSprocketsModule.cpp without them.
tpw_slice(TARGET tpw NAME tpw SOURCES sound/CSprocketsModule.cpp DROP
  __ZN16CSprocketsModule11GetPositionEjR15TbSoundPosition   # CSprocketsModule::GetPosition(unsigned int, TbSoundPosition&)
  __ZN16CSprocketsModule11StopChannelEj   # CSprocketsModule::StopChannel(unsigned int)
  __ZN16CSprocketsModule12PauseChannelEj   # CSprocketsModule::PauseChannel(unsigned int)
  __ZN16CSprocketsModule16SetChannelVolumeEjR17TbSoundSampleInfo   # CSprocketsModule::SetChannelVolume(unsigned int, TbSoundSampleInfo&)
  __ZN16CSprocketsModule16SetSpeakerConfigERKN9TbSpeaker6ConfigE   # CSprocketsModule::SetSpeakerConfig(TbSpeaker::Config const&)
  __ZN16CSprocketsModule19SetListenerPositionERK15TbSoundPosition   # CSprocketsModule::SetListenerPosition(TbSoundPosition const&)
  __ZN16CSprocketsModule7StopAllEv   # CSprocketsModule::StopAll()
  __ZN16CSprocketsModule8PauseAllEv)   # CSprocketsModule::PauseAll()

# The differential tests' libraries (tests/, cmake/units/); tpw-port turns them off when it builds this tree as a subproject.
option(TPW_BUILD_TESTS "Build the differential tests' libraries" ON)
if(TPW_BUILD_TESTS)
# Shared library with extern "C" wrappers for the Python differential tests (ctypes).
file(GLOB TPW_TEST_SHIMS CONFIGURE_DEPENDS tests/shim_*.cpp)
if(TPW_TEST_SHIMS)
  # The game functions the wrappers use, without the callees only the launcher provides (cmake/testshim.cmake). It is
  # an object library, so the wrappers' library links every kept function: a src file holds the functions of several
  # files of before the regroup, and an archive would give it only the copies something calls.
  include(cmake/testshim.cmake)
  include(cmake/legacy.cmake)
  add_library(tpw_testshim_game OBJECT ${TPW_TESTSHIM_SOURCES})
  tpw_slice(TARGET tpw_testshim_game NAME testshim KEEP ${TPW_TESTSHIM_KEEP})
  target_include_directories(tpw_testshim_game PUBLIC include)
  target_link_libraries(tpw_testshim_game PUBLIC ZLIB::ZLIB)
  add_library(tpw_testshim SHARED ${TPW_TEST_SHIMS})
  target_link_libraries(tpw_testshim PRIVATE tpw_testshim_game)
  # Older main-app tests exercise import fakes with their original call-log protocol.
  # Keep those fakes isolated once the real bullfrog string methods enter libtpw (cmake/legacy.cmake).
  # An object library too, for the same reason.
  add_library(tpw_legacy OBJECT ${TPW_LEGACY_SOURCES})
  tpw_slice(TARGET tpw_legacy NAME legacy KEEP ${TPW_LEGACY_KEEP})
  target_include_directories(tpw_legacy PUBLIC include)
  target_link_libraries(tpw_legacy PUBLIC ZLIB::ZLIB)
  set(TPW_LEGACY_SHIMS ${TPW_TEST_SHIMS})
  list(FILTER TPW_LEGACY_SHIMS EXCLUDE REGEX "/shim_string_externals\\.cpp$")
  list(FILTER TPW_LEGACY_SHIMS EXCLUDE REGEX "/shim_file_externals\\.cpp$")
  add_library(tpw_legacy_testshim SHARED ${TPW_LEGACY_SHIMS})
  target_link_libraries(tpw_legacy_testshim PRIVATE tpw_legacy)
  target_include_directories(tpw_legacy_testshim PRIVATE include tests)
  target_compile_definitions(tpw_legacy_testshim PRIVATE TPW_LEGACY_STRING_FAKES=1)
endif()

# One library per translation unit under test (tests/unit_<Name>.cpp): the unit's own
# sources (TPW_UNIT_<Name>, from src/) plus the fakes in the unit file and the call log,
# and nothing else, so no other translation can stand in for a fake. A link error names a
# callee that still needs a fake. Test modules pick one with SHIM_LIBRARY (difftest.py).
# The sets live one per file in cmake/units/<Name>.cmake (named by the unit library name, so a new
# unit adds a file there and never edits this one). A unit whose file includes tests/calllog.cpp
# itself says so there with set(TPW_UNIT_<Name>_OWN_CALLLOG ON).
file(GLOB TPW_UNIT_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/units/*.cmake)
foreach(f ${TPW_UNIT_FILES})
  include(${f})
endforeach()
file(GLOB TPW_UNIT_SHIMS CONFIGURE_DEPENDS tests/unit_*.cpp)
foreach(shim ${TPW_UNIT_SHIMS})
  get_filename_component(stem ${shim} NAME_WE)
  string(REPLACE "unit_" "" unit ${stem})
  if(TPW_UNIT_${unit}_OWN_CALLLOG)
    add_library(tpw_${stem} SHARED ${shim} ${TPW_UNIT_${unit}})
  else()
    add_library(tpw_${stem} SHARED ${shim} tests/calllog.cpp ${TPW_UNIT_${unit}})
  endif()
  target_include_directories(tpw_${stem} PRIVATE include tests)
endforeach()
# Unit w23: the bullfrog library's zlib unpacker inflates with the host's zlib, and the particle library loader's field
# swaps really swap in the test library (they do nothing on a little-endian host), so that the bytes can be compared
# with the original's.
if(TARGET tpw_unit_ParticlesInit)
  target_link_libraries(tpw_unit_ParticlesInit PRIVATE ZLIB::ZLIB)
  target_compile_definitions(tpw_unit_ParticlesInit PRIVATE TPW_SWAP_FILE_WORDS=1)
endif()
# Unit w26a: PAR_LoadLibEmitter's field swaps really swap in the test library too, so that a dropped or misplaced swap shows.
if(TARGET tpw_unit_Particles)
  target_compile_definitions(tpw_unit_Particles PRIVATE TPW_SWAP_FILE_WORDS=1)
endif()
# Unit w168: the Swap library's helpers really swap (the Save library's do not, so that its files compare byte for byte).
if(TARGET tpw_unit_ParticleSwap)
  target_compile_definitions(tpw_unit_ParticleSwap PRIVATE TPW_SWAP_FILE_WORDS=1)
endif()
# Unit v5L44: the ride height map's three byte swaps (src/game/MslSupport.cpp, src/game/online/FS_Online.cpp, src/game/park/TPHField.cpp, src/game/ui/ui_FeatureStatus.cpp) really swap in the test library, as the Mac's do.
if(TARGET tpw_unit_std_exception_what)
  target_compile_definitions(tpw_unit_std_exception_what PRIVATE TPW_SWAP_FILE_WORDS=1)
endif()
# Unit w192: the Swap library's trains save and load really swap (the plain library's do not, so that its files compare byte for byte).
if(TARGET tpw_unit_CoasterTrainsSwap)
  target_compile_definitions(tpw_unit_CoasterTrainsSwap PRIVATE TPW_SWAP_FILE_WORDS=1)
endif()
# Each unit library compiles a copy of its src files with only its kept definitions (cmake/slicer.cmake).
cmake_language(DEFER CALL tpw_slice_unit_sources)
endif()   # TPW_BUILD_TESTS

