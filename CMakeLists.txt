cmake_minimum_required(VERSION 3.25)

project(hl_server LANGUAGES C CXX)

if(NOT MSVC)
  message(FATAL_ERROR "This bootstrap currently targets MSVC/Visual Studio on Windows.")
endif()

if(CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "The Half-Life GameDLL must be built for Win32/x86. Reconfigure with the Win32 architecture.")
endif()

cmake_policy(SET CMP0091 NEW)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(HL_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/valve-halflife-sdk")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/hl_server_sources.cmake")

add_library(hl SHARED ${HL_SERVER_SOURCES})

target_compile_definitions(
  hl
  PRIVATE
    WIN32
    _WINDOWS
    QUIVER
    VOXEL
    QUAKE2
    VALVE_DLL
    CLIENT_WEAPONS
    _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

target_include_directories(
  hl
  PRIVATE
    "${HL_SDK_ROOT}/dlls"
    "${HL_SDK_ROOT}/engine"
    "${HL_SDK_ROOT}/common"
    "${HL_SDK_ROOT}/pm_shared"
    "${HL_SDK_ROOT}/game_shared"
    "${HL_SDK_ROOT}/public"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_options(
  hl
  PRIVATE
    /W3
    /GR
    $<$<CONFIG:Debug>:/Od>
    $<$<CONFIG:Release>:/O2>
    $<$<CONFIG:Release>:/Oi>
    $<$<CONFIG:Release>:/Gy>
)

target_link_options(
  hl
  PRIVATE
    "/DEF:${HL_SDK_ROOT}/dlls/hl.def"
    $<$<CONFIG:Release>:/OPT:REF>
    $<$<CONFIG:Release>:/OPT:ICF>
)

set_target_properties(
  hl
  PROPERTIES
    OUTPUT_NAME "hl"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/Release"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/Debug"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/Release"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/Release"
    PDB_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/Debug"
    PDB_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/artifacts/Release"
)
