// CSV read/write for the attitude stream (shared by monitor + attitude_publisher).
//
// AttitudeCsvLogger records each received AttitudeSample as a row, with the
// wall-clock receive time alongside the sensor's own t_ms. Rows are flushed as
// they are written so a Ctrl-C (or crash) keeps everything up to the last sample.
//
// read_attitude_csv parses that same format back into rows for replay. Only
// roll/pitch/yaw are required; seq and t_ms are optional (absent -> nullopt).
// The on-disk format matches the Python apps.attitude_log byte-for-byte.
#pragma once

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "serial_link/messages.hpp"

namespace serial_link {

// A parsed CSV row. seq / t_ms are nullopt when the column is missing or blank.
struct AttitudeRow {
    std::optional<std::uint32_t> seq;
    std::optional<std::uint32_t> t_ms;
    float roll = 0;
    float pitch = 0;
    float yaw = 0;
};

// Append attitude samples to a CSV file (header written on open).
class AttitudeCsvLogger {
public:
    explicit AttitudeCsvLogger(const std::string& path);
    ~AttitudeCsvLogger();

    // Write one sample row. `rx_unix < 0` (the default) uses the current wall clock.
    void write(const codec::AttitudeSample& sample, double rx_unix = -1.0);
    void close();

    std::size_t count() const { return count_; }
    const std::string& path() const { return path_; }

private:
    std::string path_;
    std::FILE* f_ = nullptr;
    std::size_t count_ = 0;
};

// Read a CSV into a list of AttitudeRow. Requires roll/pitch/yaw columns;
// seq/t_ms are optional. Throws std::runtime_error on missing columns / empty file.
std::vector<AttitudeRow> read_attitude_csv(const std::string& path);

}  // namespace serial_link
