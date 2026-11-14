# AVSMeter CMake build: MSVC for now; Linux (GCC, Clang) and aarch64 follow with the POSIX port.
# The hand-maintained Visual Studio 2026 project is msvc/AVSMeter.slnx; a CMake generated
# solution lives in the (gitignored) build folder only.
#
# Examples:
#   cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release   (from a VS developer prompt)
#   cmake -B build\vs64 -S . -G "Visual Studio 18 2026" -A x64
#   cmake -B build\vs32 -S . -G "Visual Studio 18 2026" -A Win32
cmake_minimum_required(VERSION 3.16...3.31)

# Before project(): CMake's MSVC default for single-config generators would be Debug
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_GENERATOR MATCHES "Visual Studio|Multi-Config|Xcode")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

project("AVSMeter" LANGUAGES CXX)
include(GNUInstallDirs)

# Avoid uselessly linking to unused libraries
set(CMAKE_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

if(NOT MSVC)
  message(FATAL_ERROR "AVSMeter builds with MSVC only, until the POSIX port is done")
endif()

# Static runtime (/MT, /MTd), same as msvc/AVSMeter.vcxproj
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# /EHa: the SEH translator in exception.cpp turns access violations etc. into C++ exceptions.
# CMake's default /EHsc must go, else Ninja gives D9025 (overriding '/EHs' with '/EHa');
# the VS generator hides the conflict.
string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
add_compile_options(/permissive- /Zc:__cplusplus /W3 /EHa /Gy)
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

set(ProgramName "AVSMeter")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ArchBits 64)
  set(ProgramName "AVSMeter64")
else()
  set(ArchBits 32)
endif()

set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/source")

set(AVSMeter_Sources
  "${SRC}/AVSMeter.cpp"
  "${SRC}/AvisynthInfo.cpp"
  "${SRC}/exception.cpp"
  "${SRC}/GPUInfo.cpp"
  "${SRC}/ProcessInfo.cpp"
  "${SRC}/SysInfo.cpp"
  "${SRC}/Timer.cpp"
  "${SRC}/Utility.cpp"
)
set(AVSMeter_Headers
  "${SRC}/AvisynthInfo.h"
  "${SRC}/common.h"
  "${SRC}/exception.h"
  "${SRC}/GPUInfo.h"
  "${SRC}/ProcessInfo.h"
  "${SRC}/SysInfo.h"
  "${SRC}/Timer.h"
  "${SRC}/Utility.h"
  "${SRC}/version.h"
  "${SRC}/platform/console.h"
  "${SRC}/platform/process_stats.h"
)

if(WIN32)
  list(APPEND AVSMeter_Sources
    "${SRC}/platform/win32/console_win32.cpp"
    "${SRC}/platform/win32/process_stats_win32.cpp"
  )
endif()

add_executable(AVSMeter ${AVSMeter_Sources} ${AVSMeter_Headers})
set_target_properties(AVSMeter PROPERTIES "OUTPUT_NAME" "${ProgramName}")

source_group("Source Files" FILES ${AVSMeter_Sources})
source_group("Header Files" FILES ${AVSMeter_Headers})

target_include_directories(AVSMeter PRIVATE "${SRC}")

if(WIN32)
  # version resource and icon
  enable_language(RC)
  target_sources(AVSMeter PRIVATE "${SRC}/AVSMeter.rc")
  set_source_files_properties("${SRC}/AVSMeter.rc" PROPERTIES COMPILE_OPTIONS "/l0x0809")
  source_group("Resource Files" FILES "${SRC}/AVSMeter.rc")

  # Windows 7 minimum
  target_compile_definitions(AVSMeter PRIVATE WIN32 _CONSOLE WINVER=0x0601 _WIN32_WINNT=0x0601)
  target_link_options(AVSMeter PRIVATE "/SUBSYSTEM:CONSOLE,6.01")
  if(ArchBits EQUAL 32)
    target_link_options(AVSMeter PRIVATE "/LARGEADDRESSAWARE")
  endif()

  # prebuilt libcpuid 0.8.2 static libraries
  target_link_libraries(AVSMeter PRIVATE
    "${SRC}/cpuid/libcpuid${ArchBits}$<$<CONFIG:Debug>:d>.lib"
    kernel32 user32 advapi32 shell32 comdlg32 psapi imagehlp version
  )
endif()

install(TARGETS AVSMeter
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
