# Toolchain: SpacemiT K1 / Orange Pi RV2 (RISC-V rv64) — cross-compile.
#
# The Orange Pi RV2 uses the SpacemiT K1 (8x X60 cores), an RVA22-class RV64GC
# part with the RVV 1.0 vector extension (VLEN=256) plus Zba/Zbb/Zbs. We compile
# for rv64gcv_zba_zbb_zbs (lp64d ABI) with the Debian riscv64-linux-gnu cross
# toolchain and run tests under qemu-user with a matching 256-bit vector unit.
#
#   cmake -S . -B build/k1 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/riscv64-spacemit-k1.cmake

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

set(_triple riscv64-linux-gnu)
set(CMAKE_C_COMPILER   ${_triple}-gcc)
set(CMAKE_CXX_COMPILER ${_triple}-g++)

set(_flags "-march=rv64gcv_zba_zbb_zbs -mabi=lp64d")
set(CMAKE_C_FLAGS_INIT   "${_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_flags}")

set(CMAKE_FIND_ROOT_PATH /usr/${_triple})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# qemu-user with RVV 1.0, VLEN=256 (the K1's vector length) + Zba/Zbb/Zbs.
set(CMAKE_CROSSCOMPILING_EMULATOR
    qemu-riscv64-static;-L;/usr/${_triple};-cpu;rv64,v=true,vlen=256,vext_spec=v1.0,zba=true,zbb=true,zbs=true)

set(SERIAL_LINK_TARGET_TAG "linux-riscv64-spacemit-k1" CACHE STRING "package target tag")
set(SERIAL_LINK_DEB_ARCH   "riscv64"                    CACHE STRING "dpkg architecture")
