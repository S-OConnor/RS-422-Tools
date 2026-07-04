# Toolchain: ARM Cortex-A72 (AArch64) — cross-compile.
#
# Targets the Cortex-A72 (Raspberry Pi 4, many A72 SBCs / SoCs). Uses the Debian
# aarch64-linux-gnu cross toolchain (package: crossbuild-essential-arm64) and runs
# any test/integration binaries through qemu-user so foreign-arch code is exercised
# on an x86 CI runner.
#
#   cmake -S . -B build/a72 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-cortex-a72.cmake

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(_triple aarch64-linux-gnu)
set(CMAKE_C_COMPILER   ${_triple}-gcc)
set(CMAKE_CXX_COMPILER ${_triple}-g++)

# -mcpu=cortex-a72 selects both the A72 ISA (armv8-a+crc) and its scheduling model.
set(_flags "-mcpu=cortex-a72")
set(CMAKE_C_FLAGS_INIT   "${_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_flags}")

# Look for libraries/headers in the cross sysroot, but find programs on the host.
set(CMAKE_FIND_ROOT_PATH /usr/${_triple})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ctest runs the cross binaries through qemu-user (also used by gtest PRE_TEST
# discovery). -L points qemu at the cross sysroot for the dynamic loader + libs.
set(CMAKE_CROSSCOMPILING_EMULATOR
    qemu-aarch64-static;-L;/usr/${_triple};-cpu;cortex-a72)

set(SERIAL_LINK_TARGET_TAG "linux-aarch64-cortex-a72" CACHE STRING "package target tag")
set(SERIAL_LINK_DEB_ARCH   "arm64"                     CACHE STRING "dpkg architecture")
