#include "serial_link/attitude_log.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace serial_link {

namespace {
double now_unix() {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}
}  // namespace

AttitudeCsvLogger::AttitudeCsvLogger(const std::string& path) : path_(path) {
    f_ = std::fopen(path.c_str(), "w");
    if (!f_) throw std::runtime_error("cannot open " + path + " for writing");
    std::fputs("rx_unix,seq,t_ms,roll,pitch,yaw\n", f_);
    std::fflush(f_);
}

AttitudeCsvLogger::~AttitudeCsvLogger() { close(); }

void AttitudeCsvLogger::write(const codec::AttitudeSample& sample, double rx_unix) {
    if (rx_unix < 0.0) rx_unix = now_unix();
    // Column order + formatting mirror the Python AttitudeCsvLogger exactly.
    std::fprintf(f_, "%.6f,%u,%u,%.4f,%.4f,%.4f\n", rx_unix, sample.seq, sample.t_ms,
                 static_cast<double>(sample.roll), static_cast<double>(sample.pitch),
                 static_cast<double>(sample.yaw));
    std::fflush(f_);
    ++count_;
}

void AttitudeCsvLogger::close() {
    if (f_) {
        std::fflush(f_);
        std::fclose(f_);
        f_ = nullptr;
    }
}

namespace {
std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> out;
    std::string field;
    std::istringstream ss(line);
    while (std::getline(ss, field, ',')) {
        // trim a trailing CR from CRLF files
        if (!field.empty() && field.back() == '\r') field.pop_back();
        out.push_back(field);
    }
    return out;
}
}  // namespace

std::vector<AttitudeRow> read_attitude_csv(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);

    std::string header;
    if (!std::getline(f, header)) {
        throw std::runtime_error("CSV '" + path + "' is empty");
    }
    std::unordered_map<std::string, int> col;
    {
        auto names = split_csv(header);
        for (int i = 0; i < static_cast<int>(names.size()); ++i) col[names[i]] = i;
    }
    for (const char* req : {"roll", "pitch", "yaw"}) {
        if (col.find(req) == col.end()) {
            throw std::runtime_error("CSV '" + path + "' is missing required column: " + req);
        }
    }
    auto has = [&](const char* c) { return col.find(c) != col.end(); };
    int c_roll = col["roll"], c_pitch = col["pitch"], c_yaw = col["yaw"];
    int c_seq = has("seq") ? col["seq"] : -1;
    int c_t_ms = has("t_ms") ? col["t_ms"] : -1;

    std::vector<AttitudeRow> rows;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto fields = split_csv(line);
        auto at = [&](int idx) -> std::string {
            return (idx >= 0 && idx < static_cast<int>(fields.size())) ? fields[idx] : "";
        };
        AttitudeRow row;
        row.roll = std::stof(at(c_roll));
        row.pitch = std::stof(at(c_pitch));
        row.yaw = std::stof(at(c_yaw));
        std::string seq = at(c_seq);
        std::string t_ms = at(c_t_ms);
        if (!seq.empty()) row.seq = static_cast<std::uint32_t>(std::stoul(seq));
        if (!t_ms.empty()) row.t_ms = static_cast<std::uint32_t>(std::stoul(t_ms));
        rows.push_back(row);
    }
    if (rows.empty()) throw std::runtime_error("CSV '" + path + "' has no data rows");
    return rows;
}

}  // namespace serial_link
