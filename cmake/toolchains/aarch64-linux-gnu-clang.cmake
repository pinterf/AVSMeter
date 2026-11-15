# Cross build for Linux aarch64 with Clang + lld; uses the GNU cross sysroot and libstdc++, e.g. on Ubuntu:
#   sudo apt install clang lld g++-aarch64-linux-gnu qemu-user
#   cmake -B build/linux-aarch64-clang -G Ninja --toolchain cmake/toolchains/aarch64-linux-gnu-clang.cmake
# Versioned compilers: add -DCMAKE_C_COMPILER=clang-21 -DCMAKE_CXX_COMPILER=clang++-21
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")

# Ubuntu/Debian cross packages (g++-aarch64-linux-gnu) install the target libraries here
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)