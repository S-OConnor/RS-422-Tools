// Mirrors tests/test_attitude_log.py — CSV write then read back, optional
// columns, and the required-column / empty-file errors.
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "serial_link/attitude_log.hpp"

using namespace serial_link;

namespace {
std::string temp_path(const char* name) {
    return std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/slcpp_" +
           name + ".csv";
}
void write_file(const std::string& path, const std::string& body) {
    std::ofstream(path) << body;
}
}  // namespace

TEST(AttitudeLog, WriteThenReadRoundtrip) {
    std::string path = temp_path("rt");
    {
        AttitudeCsvLogger log(path);
        log.write(codec::AttitudeSample{0, 0, 1.0f, 2.0f, 3.0f}, 1000.0);
        log.write(codec::AttitudeSample{1, 100, -4.5f, 5.5f, 6.5f}, 1000.1);
        EXPECT_EQ(log.count(), 2u);
    }
    auto rows = read_attitude_csv(path);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].seq.value(), 0u);
    EXPECT_EQ(rows[1].t_ms.value(), 100u);
    EXPECT_NEAR(rows[1].roll, -4.5f, 1e-4);
    EXPECT_NEAR(rows[1].yaw, 6.5f, 1e-4);
    std::remove(path.c_str());
}

TEST(AttitudeLog, OptionalColumnsAbsent) {
    std::string path = temp_path("optional");
    write_file(path, "roll,pitch,yaw\n1.0,2.0,3.0\n");
    auto rows = read_attitude_csv(path);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_FALSE(rows[0].seq.has_value());
    EXPECT_FALSE(rows[0].t_ms.has_value());
    EXPECT_NEAR(rows[0].pitch, 2.0f, 1e-4);
    std::remove(path.c_str());
}

TEST(AttitudeLog, MissingRequiredColumnThrows) {
    std::string path = temp_path("missing");
    write_file(path, "roll,pitch\n1.0,2.0\n");
    EXPECT_THROW(read_attitude_csv(path), std::runtime_error);
    std::remove(path.c_str());
}

TEST(AttitudeLog, EmptyFileThrows) {
    std::string path = temp_path("empty");
    write_file(path, "roll,pitch,yaw\n");
    EXPECT_THROW(read_attitude_csv(path), std::runtime_error);
    std::remove(path.c_str());
}
