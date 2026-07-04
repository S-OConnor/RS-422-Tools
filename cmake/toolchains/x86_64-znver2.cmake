# Toolchain: x86-64 tuned for AMD Ryzen (Zen 2 / "znver2").
#
# Native build on an x86-64 host (no cross compiler) — pins -march/-mtune to Zen 2.
# Zen 2 is the broad "Ryzen" baseline (Ryzen 3000/4000, EPYC Rome); its ISA
# (AVX2/FMA/BMI2) is a superset-compatible target that also runs on Broadwell+
# hosts, so the CI runner can execute the binaries. Bump to znver3/znver4 for a
# newer Ryzen if the deployment SKU is known.
#
#   cmake -S . -B build/zen -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-znver2.cmake

set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_flags "-march=znver2 -mtune=znver2")
set(CMAKE_C_FLAGS_INIT   "${_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_flags}")

set(SERIAL_LINK_TARGET_TAG "linux-x86_64-znver2" CACHE STRING "package target tag")
set(SERIAL_LINK_DEB_ARCH   "amd64"               CACHE STRING "dpkg architecture")
