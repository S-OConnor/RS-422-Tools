# serial_link — C++ port of the RS-422 attitude/register link for Yocto.
#
# Builds:
#   * libserial_link       static library (L0 transport -> L3 link)
#   * rs422-monitor        RX: receive + live-display the attitude stream
#   * rs422-attitude-publisher  TX: replay a recorded CSV over the link
#   * rs422-c2             register command & control host
#
# Yocto: this is a stock CMake project — inherit `cmake` in the recipe. Pass
# -DSERIAL_LINK_BUILD_TESTS=OFF so the image build never fetches GoogleTest;
# DEPEND on googletest and leave tests ON only where you actually run ptest.
cmake_minimum_required(VERSION 3.16)
project(serial_link VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SERIAL_LINK_BUILD_APPS "Build the monitor/c2/attitude_publisher apps" ON)
option(SERIAL_LINK_BUILD_TESTS "Build the C++ unit tests (needs GoogleTest)" ON)

find_package(Threads REQUIRED)

# --- library ----------------------------------------------------------------
add_library(serial_link STATIC
  src/fcs.cpp
  src/framing.cpp
  src/messages.cpp
  src/codec.cpp
  src/transport.cpp
  src/link.cpp
  src/framelog.cpp
  src/cli.cpp
  src/attitude_log.cpp
)
target_include_directories(serial_link PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_options(serial_link PRIVATE -Wall -Wextra)
# openpty() lives in libutil on glibc; std::thread/sleep_for want pthreads.
target_link_libraries(serial_link PUBLIC Threads::Threads util)

# --- apps -------------------------------------------------------------------
if(SERIAL_LINK_BUILD_APPS)
  add_executable(rs422-monitor apps/monitor/main.cpp)
  add_executable(rs422-attitude-publisher apps/attitude_publisher/main.cpp)
  add_executable(rs422-c2 apps/c2/main.cpp)
  foreach(app rs422-monitor rs422-attitude-publisher rs422-c2)
    target_link_libraries(${app} PRIVATE serial_link)
    target_compile_options(${app} PRIVATE -Wall -Wextra)
  endforeach()
  install(TARGETS rs422-monitor rs422-attitude-publisher rs422-c2
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# --- install (library + headers) --------------------------------------------
install(TARGETS serial_link ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# --- tests ------------------------------------------------------------------
if(SERIAL_LINK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# --- packaging (CPack: portable tarball + Debian .deb) ----------------------
# The active toolchain file (cmake/toolchains/<variant>.cmake) sets
# SERIAL_LINK_TARGET_TAG and SERIAL_LINK_DEB_ARCH, so each microarch build packages
# under its own tagged name (e.g. serial-link-0.1.0-linux-riscv64-sifive-x280.tar.gz).
# A plain native build with no toolchain file still produces a tarball, tagged by
# the host processor. `cpack` reads this from the build tree — see the CI package
# stage and docs/ci-cd.md.
set(CPACK_PACKAGE_NAME "serial-link")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "RS-422 attitude/register serial link — libserial_link + rs422-* apps")
set(CPACK_PACKAGE_CONTACT "stephen.r.oconnor92@gmail.com")
set(CPACK_PACKAGE_VENDOR "serial_link")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_GENERATOR "TGZ")

if(SERIAL_LINK_TARGET_TAG)
  set(CPACK_SYSTEM_NAME "${SERIAL_LINK_TARGET_TAG}")
endif()

# Emit a .deb only when the toolchain declared a dpkg architecture (the CI
# variants do). Shared-lib deps are stated explicitly so no cross dpkg-shlibdeps
# run is needed: the apps depend only on libc and libstdc++.
if(SERIAL_LINK_DEB_ARCH)
  list(APPEND CPACK_GENERATOR "DEB")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${SERIAL_LINK_DEB_ARCH}")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6")
  set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
endif()

include(CPack)
