# Cross build for Linux aarch64 with the GNU cross compiler, e.g. on Ubuntu:
#   sudo apt install g++-aarch64-linux-gnu qemu-user
#   cmake -B build/linux-aarch64-gcc -G Ninja --toolchain cmake/toolchains/aarch64-linux-gnu-gcc.cmake
# Run with: qemu-aarch64 -L /usr/aarch64-linux-gnu build/linux-aarch64-gcc/avsmeter ...
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Ubuntu/Debian cross packages (g++-aarch64-linux-gnu) install the target libraries here
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)