# AVSMeter CMake build: Windows (MSVC) and Linux (GCC, Clang).
# The hand-maintained Visual Studio 2026 project is msvc/AVSMeter.slnx; a CMake generated
# solution lives in the (gitignored) build folder only.
#
# Examples (or see CMakePresets.json: cmake --preset linux-gcc && cmake --build --preset linux-gcc):
#   cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release   (Linux, or from a VS developer prompt)
#   cmake -B build\vs64 -S . -G "Visual Studio 18 2026" -A x64
#   cmake -B build\vs32 -S . -G "Visual Studio 18 2026" -A Win32
#   cmake -B build/a64 -S . -G Ninja --toolchain cmake/toolchains/aarch64-linux-gnu-gcc.cmake  (cross)
#
# libcpuid: MSVC links the prebuilt static libraries in source/cpuid. Elsewhere the libcpuid 0.8.2
# release archive is downloaded at configure time and built as a static library;
# -DAVSMETER_SYSTEM_LIBCPUID=ON uses the system's libcpuid (pkg-config) instead.
cmake_minimum_required(VERSION 3.22...3.31)

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

option(AVSMETER_SYSTEM_LIBCPUID "Use the system's libcpuid (pkg-config) instead of downloading 0.8.2" OFF)

if(MSVC)
  # Static runtime (/MT, /MTd), same as msvc/AVSMeter.vcxproj
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

  # /EHa: the SEH translator in platform/win32/crash_guard_win32.cpp turns access violations etc. into C++ exceptions.
  # CMake's default /EHsc must go, else Ninja gives D9025 (overriding '/EHs' with '/EHa');
  # the VS generator hides the conflict.
  string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  add_compile_options(/permissive- /Zc:__cplusplus /W3 /EHa /Gy)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ArchBits 64)
else()
  set(ArchBits 32)
endif()

if(WIN32)
  if(ArchBits EQUAL 64)
    set(ProgramName "AVSMeter64")
  else()
    set(ProgramName "AVSMeter")
  endif()
else()
  set(ProgramName "avsmeter")
endif()

set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/source")

set(AVSMeter_Sources
  "${SRC}/AVSMeter.cpp"
  "${SRC}/AvisynthInfo.cpp"
  "${SRC}/ProcessInfo.cpp"
  "${SRC}/SysInfo.cpp"
  "${SRC}/Timer.cpp"
  "${SRC}/Utility.cpp"
)
set(AVSMeter_Headers
  "${SRC}/AvisynthInfo.h"
  "${SRC}/common.h"
  "${SRC}/ProcessInfo.h"
  "${SRC}/SysInfo.h"
  "${SRC}/Timer.h"
  "${SRC}/Utility.h"
  "${SRC}/version.h"
  "${SRC}/platform/console.h"
  "${SRC}/platform/dynlib.h"
  "${SRC}/platform/os_info.h"
  "${SRC}/platform/avs_registry.h"
  "${SRC}/platform/crash_guard.h"
  "${SRC}/platform/gpu_sensors.h"
  "${SRC}/platform/binary_info.h"
  "${SRC}/platform/file_info.h"
  "${SRC}/platform/process.h"
  "${SRC}/platform/process_stats.h"
  "${SRC}/platform/text.h"
  "${SRC}/platform/single_instance.h"
)

if(WIN32)
  list(APPEND AVSMeter_Sources
    "${SRC}/platform/win32/console_win32.cpp"
    "${SRC}/platform/win32/dynlib_win32.cpp"
    "${SRC}/platform/win32/os_info_win32.cpp"
    "${SRC}/platform/win32/avs_registry_win32.cpp"
    "${SRC}/platform/win32/crash_guard_win32.cpp"
    "${SRC}/platform/win32/gpu_sensors_win32.cpp"
    "${SRC}/platform/win32/binary_info_win32.cpp"
    "${SRC}/platform/win32/file_info_win32.cpp"
    "${SRC}/platform/win32/process_win32.cpp"
    "${SRC}/platform/win32/text_win32.cpp"
    "${SRC}/platform/win32/utf_win32.h"
    "${SRC}/platform/win32/process_stats_win32.cpp"
    "${SRC}/platform/win32/single_instance_win32.cpp"
  )
else()
  list(APPEND AVSMeter_Sources
    "${SRC}/platform/posix/console_posix.cpp"
    "${SRC}/platform/posix/console_posix.h"
    "${SRC}/platform/posix/dynlib_posix.cpp"
    "${SRC}/platform/posix/os_info_posix.cpp"
    "${SRC}/platform/posix/avs_registry_posix.cpp"
    "${SRC}/platform/posix/crash_guard_posix.cpp"
    "${SRC}/platform/posix/gpu_sensors_posix.cpp"
    "${SRC}/platform/posix/binary_info_posix.cpp"
    "${SRC}/platform/posix/file_info_posix.cpp"
    "${SRC}/platform/posix/process_posix.cpp"
    "${SRC}/platform/posix/text_posix.cpp"
    "${SRC}/platform/posix/last_error_posix.h"
    "${SRC}/platform/posix/process_stats_posix.cpp"
    "${SRC}/platform/posix/single_instance_posix.cpp"
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

  target_link_libraries(AVSMeter PRIVATE kernel32 user32 advapi32 shell32 psapi version)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(AVSMeter PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

if(MSVC)
  # prebuilt libcpuid 0.8.2 static libraries
  target_include_directories(AVSMeter PRIVATE "${SRC}/cpuid")
  target_link_libraries(AVSMeter PRIVATE "${SRC}/cpuid/libcpuid${ArchBits}$<$<CONFIG:Debug>:d>.lib")
elseif(AVSMETER_SYSTEM_LIBCPUID)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBCPUID REQUIRED IMPORTED_TARGET libcpuid)
  target_link_libraries(AVSMeter PRIVATE PkgConfig::LIBCPUID)
else()
  # Only the sources are fetched (SOURCE_SUBDIR has no CMakeLists.txt): libcpuid's own CMake
  # would also build cpuid_tool and add its install rules.
  # The release tarball, not git: the libcpuid repository keeps its test data in git-lfs.
  enable_language(C)
  include(FetchContent)
  FetchContent_Declare(libcpuid
    URL            https://github.com/anrieff/libcpuid/archive/refs/tags/v0.8.2.tar.gz
    URL_HASH       SHA256=f23e212e22ca22942cca87b18decdbce8a76d2b004c344a0789d0d26d14930d6
    SOURCE_SUBDIR  no-cmake
  )
  FetchContent_MakeAvailable(libcpuid)

  set(CPUID_SRC "${libcpuid_SOURCE_DIR}/libcpuid")
  add_library(cpuid STATIC
    "${CPUID_SRC}/cpuid_main.c"
    "${CPUID_SRC}/recog_amd.c"
    "${CPUID_SRC}/recog_arm.c"
    "${CPUID_SRC}/recog_centaur.c"
    "${CPUID_SRC}/recog_intel.c"
    "${CPUID_SRC}/rdtsc.c"
    "${CPUID_SRC}/libcpuid_util.c"
    "${CPUID_SRC}/rdmsr.c"
    "${CPUID_SRC}/asm-bits.c"
    "${CPUID_SRC}/rdcpuid.c"
  )
  set_target_properties(cpuid PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
  target_include_directories(cpuid SYSTEM PUBLIC "${CPUID_SRC}")
  target_compile_definitions(cpuid PRIVATE VERSION="0.8.2" _GNU_SOURCE)
  include(CheckSymbolExists)
  check_symbol_exists(getauxval "sys/auxv.h" HAVE_GETAUXVAL)
  if(HAVE_GETAUXVAL)
    target_compile_definitions(cpuid PRIVATE HAVE_GETAUXVAL)
  endif()
  target_link_libraries(cpuid PRIVATE Threads::Threads)
  target_link_libraries(AVSMeter PRIVATE cpuid)
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
