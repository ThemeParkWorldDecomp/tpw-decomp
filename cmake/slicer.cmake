# Copies of src files with only some of their external definitions (devtools/slicer.py). A file of src/ holds every
# function of one original source file, but a unit library builds for real only the external definitions in its keep
# list (TPW_UNIT_<Name>_KEEP in cmake/units/<Name>.cmake) and fakes the rest in tests/unit_<Name>.cpp, and a few other
# targets (tpw_legacy, the platform tests) build only some functions of a file. Such a target does not compile the file
# as it is: it compiles a copy in the build folder in which every other external definition is a declaration. The
# slicer indexes each file once per set of flags (with the compiler's own libclang, the target's definitions and
# include folders) and makes each target's copy from the index and its list. A unit without a keep list keeps nothing
# of its src files.
#
# tpw_slice(TARGET <target> NAME <name> KEEP|DROP <definition>... [SOURCES <path under src/>...]) gives <target> the
# copies of its src files (of the given SOURCES only, when there are any) that keep the KEEP definitions, or every
# definition but the DROP ones. It runs deferred, at the end of the folder that calls it, so that it sees the target's
# final sources, definitions and include folders (tpw-port puts its overrides in place before then). The copies go to
# <build folder>/slice/<name>/src/.
#
# tpw_slice_unit_sources slices every unit library, deferred to the end of the top-level CMakeLists.txt, after the
# units' own deferred target_compile_definitions. A unit file that includes src files itself (#include "src/<path>")
# names them in TPW_UNIT_<Name>_INCLUDED, with their keep list in TPW_UNIT_<Name>_INCLUDED_KEEP: their copies go to
# <build folder>/slice/<Name>_included/src/, which is on the unit library's include path, and are not compiled alone.

set_property(GLOBAL PROPERTY TPW_SLICER ${CMAKE_CURRENT_LIST_DIR}/../devtools/slicer.py)

# The path of a source under src/ (of this tree, or of tpw-port's overrides/src), or "" for any other source.
function(_tpw_slice_src_path source result)
  set(path "")
  if(source MATCHES "^src/(.+)$")
    set(path ${CMAKE_MATCH_1})
  elseif(source MATCHES "/src/(.+)$")
    set(path ${CMAKE_MATCH_1})
  endif()
  set(${result} "${path}" PARENT_SCOPE)
endfunction()

# tpw_test_source_flags(<src/path> [DEFINITIONS <def>...] [OPTIONS <option>...]) gives every test copy of one src file
# (a unit library's, the test shim's, the legacy library's) compile definitions and options that the real file must not
# get. A unit's cmake file calls it where it used to call set_source_files_properties(src/... COMPILE_DEFINITIONS|
# COMPILE_OPTIONS): that is scoped to the folder, so the game library (tpw) compiled the file with the setting too and
# lost, for one, the real forwarders of the callees the test library fakes.
function(tpw_test_source_flags source)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "DEFINITIONS;OPTIONS")
  _tpw_slice_src_path("${source}" rel)
  if(rel STREQUAL "")
    message(FATAL_ERROR "tpw_test_source_flags: ${source} is not a file of src/")
  endif()
  string(MAKE_C_IDENTIFIER "${rel}" key)
  if(arg_DEFINITIONS)
    set_property(GLOBAL APPEND PROPERTY TPW_TEST_SOURCE_DEFINITIONS_${key} ${arg_DEFINITIONS})
  endif()
  if(arg_OPTIONS)
    set_property(GLOBAL APPEND PROPERTY TPW_TEST_SOURCE_OPTIONS_${key} ${arg_OPTIONS})
  endif()
endfunction()

function(_tpw_slice_target target name mode list only included)
  get_property(slicer GLOBAL PROPERTY TPW_SLICER)
  set(out ${CMAKE_CURRENT_BINARY_DIR}/slice)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" config)
  separate_arguments(base_flags NATIVE_COMMAND "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${config}}")
  list(PREPEND base_flags -std=c++${CMAKE_CXX_STANDARD})
  get_target_property(target_dir ${target} SOURCE_DIR)
  get_directory_property(dir_defs DIRECTORY ${target_dir} COMPILE_DEFINITIONS)
  get_target_property(sources ${target} SOURCES)
  get_target_property(target_defs ${target} COMPILE_DEFINITIONS)
  get_target_property(incs ${target} INCLUDE_DIRECTORIES)
  set(defs ${dir_defs})
  if(target_defs)
    list(APPEND defs ${target_defs})
  endif()
  list(REMOVE_DUPLICATES defs)
  if(defs MATCHES "\\$<" OR incs MATCHES "\\$<")
    message(FATAL_ERROR "slicer: ${target} has a generator expression the index cannot follow")
  endif()
  list(REMOVE_DUPLICATES incs)

  string(TOLOWER ${mode} option)
  set(list_file ${out}/${name}/${option}.txt)
  list(JOIN list "\n" list_text)
  set(old_text "")
  if(EXISTS ${list_file})
    file(READ ${list_file} old_text)
  endif()
  if(NOT old_text STREQUAL "${list_text}\n")
    file(WRITE ${list_file} "${list_text}\n")
  endif()

  if(included)
    set(sources ${included})
  endif()
  set(new_sources)
  foreach(src ${sources})
    _tpw_slice_src_path("${src}" rel)
    if(rel STREQUAL "" OR (only AND NOT rel IN_LIST only))
      list(APPEND new_sources ${src})
      continue()
    endif()
    if(IS_ABSOLUTE ${src})
      set(abs ${src})
    else()
      set(abs ${target_dir}/${src})
    endif()
    get_source_file_property(src_defs ${src} TARGET_DIRECTORY ${target} COMPILE_DEFINITIONS)
    get_source_file_property(src_opts ${src} TARGET_DIRECTORY ${target} COMPILE_OPTIONS)
    string(MAKE_C_IDENTIFIER "${rel}" rel_key)
    get_property(test_defs GLOBAL PROPERTY TPW_TEST_SOURCE_DEFINITIONS_${rel_key})
    get_property(test_opts GLOBAL PROPERTY TPW_TEST_SOURCE_OPTIONS_${rel_key})
    if(NOT src_defs)
      set(src_defs)
    endif()
    if(NOT src_opts)
      set(src_opts)
    endif()
    list(APPEND src_defs ${test_defs})
    list(APPEND src_opts ${test_opts})
    set(flags ${base_flags})
    foreach(d ${defs})
      list(APPEND flags -D${d})
    endforeach()
    if(src_defs)
      foreach(d ${src_defs})
        list(APPEND flags -D${d})
      endforeach()
    endif()
    foreach(i ${incs})
      list(APPEND flags -I${i})
    endforeach()
    string(SHA1 key "${abs} ${flags}")
    string(SUBSTRING ${key} 0 12 key)
    set(index ${out}/index/src/${rel}.${key}.json)
    get_property(indexed GLOBAL PROPERTY TPW_SLICE_INDEXES)
    if(NOT index IN_LIST indexed)
      set_property(GLOBAL APPEND PROPERTY TPW_SLICE_INDEXES ${index})
      add_custom_command(OUTPUT ${index}
        COMMAND ${Python3_EXECUTABLE} -I ${slicer} index --src ${abs} --out ${index} --depfile ${index}.d
                -- ${CMAKE_CXX_COMPILER} ${flags}
        DEPENDS ${abs} ${slicer}
        DEPFILE ${index}.d
        COMMENT "Indexing src/${rel}"
        VERBATIM)
    endif()
    set(copy ${out}/${name}/src/${rel})
    add_custom_command(OUTPUT ${copy}
      COMMAND ${Python3_EXECUTABLE} -I ${slicer} slice --src ${abs} --index ${index} --${option} ${list_file} --out ${copy}
      DEPENDS ${abs} ${index} ${list_file} ${slicer}
      COMMENT "Slicing src/${rel} for ${name}"
      VERBATIM)
    # The copy is compiled as the file would be, with its own definitions and options; its quoted includes are found
    # next to the original. The static helpers a copy no longer calls are not warned about.
    get_filename_component(dir ${abs} DIRECTORY)
    set(opts -iquote ${dir} -Wno-unused-function -Wno-unused-variable -Wno-unused-const-variable)
    if(src_opts)
      list(PREPEND opts ${src_opts})
    endif()
    set_source_files_properties(${copy} TARGET_DIRECTORY ${target} PROPERTIES COMPILE_OPTIONS "${opts}")
    if(src_defs)
      set_source_files_properties(${copy} TARGET_DIRECTORY ${target} PROPERTIES COMPILE_DEFINITIONS "${src_defs}")
    endif()
    list(APPEND new_sources ${copy})
  endforeach()
  if(included)
    set_source_files_properties(${new_sources} TARGET_DIRECTORY ${target} PROPERTIES HEADER_FILE_ONLY ON)
    target_sources(${target} PRIVATE ${new_sources})
    target_include_directories(${target} PRIVATE ${out}/${name})
  else()
    set_property(TARGET ${target} PROPERTY SOURCES ${new_sources})
  endif()
endfunction()

function(_tpw_slice_run name)
  foreach(field TARGET MODE LIST SOURCES)
    get_property(${field} GLOBAL PROPERTY TPW_SLICE_${name}_${field})
  endforeach()
  _tpw_slice_target(${TARGET} ${name} ${MODE} "${LIST}" "${SOURCES}" "")
endfunction()

function(tpw_slice)
  cmake_parse_arguments(PARSE_ARGV 0 arg "" "TARGET;NAME" "KEEP;DROP;SOURCES")
  if(DEFINED arg_KEEP AND NOT DEFINED arg_DROP)
    set(mode KEEP)
  elseif(DEFINED arg_DROP AND NOT DEFINED arg_KEEP)
    set(mode DROP)
  else()
    message(FATAL_ERROR "tpw_slice(${arg_TARGET}): give KEEP or DROP")
  endif()
  set_property(GLOBAL PROPERTY TPW_SLICE_${arg_NAME}_TARGET ${arg_TARGET})
  set_property(GLOBAL PROPERTY TPW_SLICE_${arg_NAME}_MODE ${mode})
  set_property(GLOBAL PROPERTY TPW_SLICE_${arg_NAME}_LIST "${arg_${mode}}")
  set_property(GLOBAL PROPERTY TPW_SLICE_${arg_NAME}_SOURCES "${arg_SOURCES}")
  cmake_language(EVAL CODE "cmake_language(DEFER CALL _tpw_slice_run ${arg_NAME})")
endfunction()

function(tpw_slice_unit_sources)
  foreach(shim ${TPW_UNIT_SHIMS})
    get_filename_component(stem ${shim} NAME_WE)
    string(REPLACE "unit_" "" unit ${stem})
    _tpw_slice_target(tpw_${stem} ${unit} KEEP "${TPW_UNIT_${unit}_KEEP}" "" "")
    if(TPW_UNIT_${unit}_INCLUDED)
      _tpw_slice_target(tpw_${stem} ${unit}_included KEEP "${TPW_UNIT_${unit}_INCLUDED_KEEP}" ""
                        "${TPW_UNIT_${unit}_INCLUDED}")
    endif()
  endforeach()
endfunction()
