# Toolchain: SiFive Intelligence X280 (RISC-V rv64) — cross-compile.
#
# The X280 is an RV64GC application core with the RVV 1.0 vector extension
# (VLEN=512) plus the Zba/Zbb bit-manipulation extensions. We compile for
# rv64gcv_zba_zbb (lp64d ABI) with the Debian riscv64-linux-gnu cross toolchain
# (package: crossbuild-essential-riscv64) and run tests under qemu-user configured
# with a matching 512-bit vector unit.
#
# NOTE: some GCCs also accept `-mtune=sifive-x280` / `-mcpu=sifive-x280`; it is
# left off here to stay portable across toolchain versions. Add it if your GCC has
# it — it only affects scheduling, not the ISA.
#
#   cmake -S . -B build/x280 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/riscv64-sifive-x280.cmake

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

set(_triple riscv64-linux-gnu)
set(CMAKE_C_COMPILER   ${_triple}-gcc)
set(CMAKE_CXX_COMPILER ${_triple}-g++)

set(_flags "-march=rv64gcv_zba_zbb -mabi=lp64d")
set(CMAKE_C_FLAGS_INIT   "${_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_flags}")

set(CMAKE_FIND_ROOT_PATH /usr/${_triple})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# qemu-user with RVV 1.0, VLEN=512 (the X280's vector length) + Zba/Zbb.
set(CMAKE_CROSSCOMPILING_EMULATOR
    qemu-riscv64-static;-L;/usr/${_triple};-cpu;rv64,v=true,vlen=512,vext_spec=v1.0,zba=true,zbb=true)

set(SERIAL_LINK_TARGET_TAG "linux-riscv64-sifive-x280" CACHE STRING "package target tag")
set(SERIAL_LINK_DEB_ARCH   "riscv64"                    CACHE STRING "dpkg architecture")
