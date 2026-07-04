# C++ unit tests (GoogleTest). Prefers a system/Yocto-provided GTest; falls back
# to fetching it for a local developer build only. In a Yocto image build, set
# -DSERIAL_LINK_BUILD_TESTS=OFF (or DEPEND on googletest for a ptest package).
find_package(GTest QUIET)

if(NOT GTest_FOUND)
  message(STATUS "GoogleTest not found — fetching it (developer build).")
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
  )
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(serial_link_tests
  test_fcs.cpp
  test_framing.cpp
  test_attitude_codec.cpp
  test_register_codec.cpp
  test_link_loopback.cpp
  test_attitude_log.cpp
)
target_link_libraries(serial_link_tests PRIVATE serial_link GTest::gtest_main)
target_compile_options(serial_link_tests PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(serial_link_tests)
