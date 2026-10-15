cmake_minimum_required(VERSION 3.20)
project(FreeWalk CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # The runtime is linked statically into the DLL, so the user does not have to install anything separately.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  # The source files are UTF-8 encoded.
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/W4> $<$<COMPILE_LANGUAGE:CXX>:/permissive-> $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
else()
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall> $<$<COMPILE_LANGUAGE:CXX>:-Wextra> $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>)
endif()
if(MINGW)
  # MinGW's own printf is needed for the %zu and %llu formats.
  add_compile_definitions(__USE_MINGW_ANSI_STDIO=1)
endif()

add_library(freewalk_core STATIC
  src/core/coords.cpp
  src/core/text.cpp
  src/core/config.cpp
  src/core/keys.cpp
  src/core/walker.cpp
  src/core/walk_session.cpp
  src/core/key_filter.cpp
  src/core/pe_image.cpp
  src/core/file_image.cpp
  src/core/locator.cpp
  src/core/recovery.cpp
  src/core/settings.cpp
  src/core/motion.cpp
  src/core/scene_offset.cpp
  src/core/object_check.cpp
  src/core/focus_schedule.cpp
  src/core/light_borrow.cpp
  src/core/flicker.cpp
  src/core/flashlight.cpp
  src/core/door_path.cpp
  src/core/door_choice.cpp
  src/core/door_animation.cpp
  src/core/hint.cpp
)
target_include_directories(freewalk_core PUBLIC src)

if(WIN32)
  add_library(FreeWalk SHARED
    src/win/settings_window.cpp
    src/win/plugin.cpp
    src/win/log.cpp
    src/win/process_memory.cpp
    src/win/input.cpp
    src/win/fade_window.cpp
    src/win/hint_window.cpp
    src/win/driver_hider.cpp
    src/win/game_camera.cpp
    src/win/physx_scene.cpp
    src/win/guarded_call.cpp
    src/win/region_focus.cpp
    src/win/game_lights.cpp
    src/win/FreeWalk.def
  )
  # As system headers, so the external SDK headers do not raise warnings under the strict settings.
  target_include_directories(FreeWalk SYSTEM PRIVATE third_party/scs_sdk/include)
  target_link_libraries(FreeWalk PRIVATE freewalk_core dinput8 dxguid uuid gdiplus psapi shell32 ole32)
  set_target_properties(FreeWalk PROPERTIES PREFIX "")
  if(MINGW)
    # Every runtime goes into the DLL, so the user does not have to install anything separately; without symbols (-s) the DLL stays small.
    target_link_options(FreeWalk PRIVATE -static -static-libgcc -static-libstdc++ -s)
  endif()
  if(MSVC)
    # Generate a PDB in Release too, so we can debug by attaching to the running game; it does not make the DLL bigger.
    target_compile_options(FreeWalk PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(FreeWalk PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
  endif()
endif()

# The tests and the analysis tool run on the host, so they are not built when cross-compiling.
if(NOT CMAKE_CROSSCOMPILING)
  enable_testing()
  add_executable(freewalk_tests
    tests/test_main.cpp
    tests/test_vec.cpp
    tests/test_coords.cpp
    tests/test_config.cpp
    tests/test_keys.cpp
    tests/test_walker.cpp
    tests/test_walk_session.cpp
    tests/test_key_filter.cpp
    tests/test_pe_image.cpp
    tests/test_locator.cpp
    tests/test_recovery.cpp
    tests/test_settings.cpp
    tests/test_motion.cpp
    tests/test_scene_offset.cpp
    tests/test_object_check.cpp
    tests/test_focus_schedule.cpp
    tests/test_light_borrow.cpp
    tests/test_flicker.cpp
    tests/test_flashlight.cpp
    tests/test_door_path.cpp
    tests/test_door_animation.cpp
    tests/test_hint.cpp
  )
  target_include_directories(freewalk_tests PRIVATE tests)
  # The tests compare the packaged INI with the output of the formatter.
  target_compile_definitions(freewalk_tests PRIVATE FREEWALK_INI_PATH="${CMAKE_CURRENT_SOURCE_DIR}/FreeWalk.ini")
  target_link_libraries(freewalk_tests PRIVATE freewalk_core)
  add_test(NAME freewalk_tests COMMAND freewalk_tests)

  add_executable(freewalk_probe tools/probe.cpp)
  target_link_libraries(freewalk_probe PRIVATE freewalk_core)
endif()
