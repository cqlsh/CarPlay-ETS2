cmake_minimum_required(VERSION 3.25...4.1)

# The deployment target only takes effect when set before the first project() call.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Oldest macOS release the plugin runs on")
endif()

project(CarPlay
  VERSION 0.1.0
  DESCRIPTION "CarPlay on the navigation screen of Euro Truck Simulator 2"
  LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed. Configure into a separate build directory.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "CarPlay requires a 64-bit toolchain.")
endif()

if(WIN32)
  set(CARPLAY_PORT_DIRECTORY "Windows")
elseif(APPLE)
  set(CARPLAY_PORT_DIRECTORY "Mac")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CARPLAY_PORT_DIRECTORY "Linux")
else()
  message(FATAL_ERROR "Unsupported target system: ${CMAKE_SYSTEM_NAME}")
endif()

option(CARPLAY_SKIN_IOS "Compile the Apple CarPlay skin into the plugin" ON)
option(CARPLAY_SKIN_ANDROID "Compile the Android Auto skin into the plugin" OFF)
option(CARPLAY_BUILD_CHECKS "Build and register the check runner" ON)
option(CARPLAY_WARNINGS_AS_ERRORS "Treat compiler and linker warnings as errors" ON)
option(CARPLAY_ENABLE_AVX2 "Allow AVX2 and FMA code generation on x86-64 targets" ON)
option(CARPLAY_ENABLE_LTO "Use link-time optimization for Release builds" ON)

if(NOT CARPLAY_SKIN_IOS AND NOT CARPLAY_SKIN_ANDROID)
  message(FATAL_ERROR "At least one skin must be enabled.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ${CARPLAY_WARNINGS_AS_ERRORS})
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

get_property(CARPLAY_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT CARPLAY_IS_MULTI_CONFIG)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
  endif()
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,Release>:ProgramDatabase>")
  # CMake seeds the Release flags with /Ob2, which would clash with /Ob3 on the command line.
  string(REPLACE "/Ob2" "/Ob3" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

if(CARPLAY_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT CARPLAY_LTO_SUPPORTED OUTPUT CARPLAY_LTO_MESSAGE LANGUAGES CXX)
  if(CARPLAY_LTO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  else()
    message(STATUS "CarPlay: link-time optimization is not available: ${CARPLAY_LTO_MESSAGE}")
  endif()
endif()

set(CARPLAY_IS_X64 OFF)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
  set(CARPLAY_IS_X64 ON)
endif()

set(CARPLAY_BUILD_COMMIT "unknown")
find_package(Git QUIET)
if(Git_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE CARPLAY_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE CARPLAY_GIT_RESULT
    ERROR_QUIET)
  if(CARPLAY_GIT_RESULT EQUAL 0 AND CARPLAY_GIT_COMMIT)
    set(CARPLAY_BUILD_COMMIT "${CARPLAY_GIT_COMMIT}")
  endif()
endif()

add_library(carplay_settings INTERFACE)

target_include_directories(carplay_settings INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(carplay_settings INTERFACE
  CARPLAY_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  CARPLAY_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  CARPLAY_VERSION_PATCH=${PROJECT_VERSION_PATCH}
  CARPLAY_VERSION_TEXT="${PROJECT_VERSION}"
  CARPLAY_BUILD_COMMIT="${CARPLAY_BUILD_COMMIT}"
  CARPLAY_SKIN_IOS=$<BOOL:${CARPLAY_SKIN_IOS}>
  CARPLAY_SKIN_ANDROID=$<BOOL:${CARPLAY_SKIN_ANDROID}>
  CARPLAY_DEBUG_BUILD=$<CONFIG:Debug>)

if(MSVC)
  target_compile_definitions(carplay_settings INTERFACE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    STRICT
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00)
  target_compile_options(carplay_settings INTERFACE
    /W4
    /permissive-
    /Zc:__cplusplus
    /Zc:preprocessor
    /Zc:inline
    /Zc:throwingNew
    /utf-8
    /GR-
    /diagnostics:caret
    /external:anglebrackets
    /external:W0
    /w14242 /w14254 /w14263 /w14265 /w14287 /w14296 /w14311
    /w14545 /w14546 /w14547 /w14549 /w14555 /w14619 /w14640
    /w14826 /w14905 /w14906 /w14928
    $<$<CONFIG:Release>:/Oi;/Gy;/Gw>
    $<$<AND:$<BOOL:${CARPLAY_ENABLE_AVX2}>,$<BOOL:${CARPLAY_IS_X64}>>:/arch:AVX2>)
  target_link_options(carplay_settings INTERFACE
    $<$<CONFIG:Release>:/DEBUG;/OPT:REF;/OPT:ICF>
    $<$<BOOL:${CARPLAY_WARNINGS_AS_ERRORS}>:/WX>)
  target_link_libraries(carplay_settings INTERFACE
    d3d11 dxgi dxguid user32 gdi32 ole32 oleaut32 uuid runtimeobject dcomp dwmapi advapi32 shell32)
else()
  target_compile_options(carplay_settings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wold-style-cast
    -Wcast-align
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
    -fno-rtti
    $<$<CONFIG:Release>:-g>
    $<$<AND:$<BOOL:${CARPLAY_ENABLE_AVX2}>,$<BOOL:${CARPLAY_IS_X64}>>:-mavx2;-mfma>)
  if(APPLE)
    target_link_libraries(carplay_settings INTERFACE
      "-framework Foundation"
      "-framework Cocoa"
      "-framework QuartzCore"
      "-framework Metal"
      "-framework WebKit"
      "-framework ScreenCaptureKit")
  else()
    find_package(Threads REQUIRED)
    target_link_libraries(carplay_settings INTERFACE Threads::Threads ${CMAKE_DL_LIBS})
  endif()
endif()

set(CARPLAY_SYSTEM_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/system")
set(CARPLAY_IOS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ios")
set(CARPLAY_ANDROID_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/android")

set(CARPLAY_SOURCE_PATTERNS "*.cpp")
if(APPLE)
  enable_language(OBJCXX)
  list(APPEND CARPLAY_SOURCE_PATTERNS "*.mm")
endif()

function(carplay_collect_sources out_variable directory)
  set(patterns)
  foreach(pattern IN LISTS CARPLAY_SOURCE_PATTERNS)
    list(APPEND patterns "${directory}/${pattern}")
  endforeach()
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${patterns})
  list(FILTER sources EXCLUDE REGEX "/Checks/")
  foreach(port IN ITEMS Windows Linux Mac)
    if(NOT port STREQUAL CARPLAY_PORT_DIRECTORY)
      list(FILTER sources EXCLUDE REGEX "/Port/${port}/")
    endif()
  endforeach()
  set(${out_variable} ${sources} PARENT_SCOPE)
endfunction()

function(carplay_collect_checks out_variable directory)
  set(patterns)
  foreach(pattern IN LISTS CARPLAY_SOURCE_PATTERNS)
    list(APPEND patterns "${directory}/Checks/${pattern}")
  endforeach()
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${patterns})
  set(${out_variable} ${sources} PARENT_SCOPE)
endfunction()

carplay_collect_sources(CARPLAY_SYSTEM_SOURCES "${CARPLAY_SYSTEM_DIRECTORY}")
carplay_collect_checks(CARPLAY_CHECK_SOURCES "${CARPLAY_SYSTEM_DIRECTORY}")

set(CARPLAY_SKIN_SOURCES)
if(CARPLAY_SKIN_IOS)
  carplay_collect_sources(CARPLAY_IOS_SOURCES "${CARPLAY_IOS_DIRECTORY}")
  carplay_collect_checks(CARPLAY_IOS_CHECK_SOURCES "${CARPLAY_IOS_DIRECTORY}")
  list(APPEND CARPLAY_SKIN_SOURCES ${CARPLAY_IOS_SOURCES})
  list(APPEND CARPLAY_CHECK_SOURCES ${CARPLAY_IOS_CHECK_SOURCES})
endif()
if(CARPLAY_SKIN_ANDROID)
  carplay_collect_sources(CARPLAY_ANDROID_SOURCES "${CARPLAY_ANDROID_DIRECTORY}")
  carplay_collect_checks(CARPLAY_ANDROID_CHECK_SOURCES "${CARPLAY_ANDROID_DIRECTORY}")
  list(APPEND CARPLAY_SKIN_SOURCES ${CARPLAY_ANDROID_SOURCES})
  list(APPEND CARPLAY_CHECK_SOURCES ${CARPLAY_ANDROID_CHECK_SOURCES})
endif()

set(CARPLAY_PLUGIN_SOURCES ${CARPLAY_SYSTEM_SOURCES} ${CARPLAY_SKIN_SOURCES})
list(LENGTH CARPLAY_PLUGIN_SOURCES CARPLAY_PLUGIN_SOURCE_COUNT)
list(LENGTH CARPLAY_CHECK_SOURCES CARPLAY_CHECK_SOURCE_COUNT)

if(CARPLAY_PLUGIN_SOURCE_COUNT GREATER 0)
  add_library(carplay_objects OBJECT ${CARPLAY_PLUGIN_SOURCES})
  target_link_libraries(carplay_objects PUBLIC carplay_settings)
  set_target_properties(carplay_objects PROPERTIES FOLDER "Plugin")
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${CARPLAY_PLUGIN_SOURCES})

  add_library(carplay SHARED $<TARGET_OBJECTS:carplay_objects>)
  target_link_libraries(carplay PRIVATE carplay_settings)
  set_target_properties(carplay PROPERTIES
    OUTPUT_NAME "carplay"
    PREFIX ""
    FOLDER "Plugin")
  if(APPLE)
    set_target_properties(carplay PROPERTIES SUFFIX ".dylib")
  endif()

  install(TARGETS carplay
    RUNTIME DESTINATION plugins
    LIBRARY DESTINATION plugins)
  install(FILES "${CARPLAY_SYSTEM_DIRECTORY}/Defaults/carplay.ini" DESTINATION plugins OPTIONAL)
  if(CARPLAY_SKIN_IOS)
    install(DIRECTORY "${CARPLAY_IOS_DIRECTORY}/Resources/" DESTINATION plugins/carplay/ios OPTIONAL)
  endif()
  if(CARPLAY_SKIN_ANDROID)
    install(DIRECTORY "${CARPLAY_ANDROID_DIRECTORY}/Resources/" DESTINATION plugins/carplay/android OPTIONAL)
  endif()
else()
  message(STATUS "CarPlay: no plugin sources yet, the plugin target is skipped")
endif()

if(CARPLAY_BUILD_CHECKS)
  if(CARPLAY_CHECK_SOURCE_COUNT GREATER 0)
    enable_testing()
    add_executable(carplay_checks ${CARPLAY_CHECK_SOURCES})
    target_link_libraries(carplay_checks PRIVATE carplay_settings)
    if(TARGET carplay_objects)
      target_sources(carplay_checks PRIVATE $<TARGET_OBJECTS:carplay_objects>)
    endif()
    set_target_properties(carplay_checks PROPERTIES FOLDER "Checks")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${CARPLAY_CHECK_SOURCES})
    add_test(NAME carplay_checks COMMAND carplay_checks)
  else()
    message(STATUS "CarPlay: no check sources yet, the check runner is skipped")
  endif()
endif()

message(STATUS "CarPlay ${PROJECT_VERSION} (${CARPLAY_BUILD_COMMIT}): "
  "system ${CMAKE_SYSTEM_NAME}, port ${CARPLAY_PORT_DIRECTORY}, "
  "skins ios=${CARPLAY_SKIN_IOS} android=${CARPLAY_SKIN_ANDROID}, "
  "plugin sources ${CARPLAY_PLUGIN_SOURCE_COUNT}, check sources ${CARPLAY_CHECK_SOURCE_COUNT}")