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
