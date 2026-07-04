# Toolchain: x86-64 tuned for Intel Broadwell.
#
# Native build on an x86-64 host (no cross compiler) — we only pin -march/-mtune
# so the binaries use the Broadwell instruction set (AVX2/FMA/BMI2 baseline).
# Consumed by CMAKE_TOOLCHAIN_FILE; pairs with the CI "x86_64-broadwell" variant.
#
#   cmake -S . -B build/bw -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-broadwell.cmake

set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_flags "-march=broadwell -mtune=broadwell")
set(CMAKE_C_FLAGS_INIT   "${_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_flags}")

# Identity used by the CPack packaging in the top-level CMakeLists.
set(SERIAL_LINK_TARGET_TAG "linux-x86_64-broadwell" CACHE STRING "package target tag")
set(SERIAL_LINK_DEB_ARCH   "amd64"                  CACHE STRING "dpkg architecture")
