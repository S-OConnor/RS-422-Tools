// attitude_publisher — replay recorded attitude out over the link.
//
// The inverse of monitor: read attitude samples from a CSV file and stream them
// out as AttitudeSample frames over RS-422 (or a stand-in transport). Reads the
// same CSV format monitor --log writes, so a recording round-trips.
//
//   rs422-attitude-publisher --port /dev/ttyUSB0 attitude.csv
//   rs422-attitude-publisher --tcp 127.0.0.1:5555 --realtime flight.csv
//   rs422-attitude-publisher --pty --loop attitude.csv
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#include "serial_link/attitude_log.hpp"
#include "serial_link/cli.hpp"
#include "serial_link/link.hpp"

using namespace serial_link;

namespace {
std::atomic<bool> g_stop{false};
void on_sigint(int) { g_stop = true; }

void sleep_seconds(double dt) {
    if (dt > 0) std::this_thread::sleep_for(std::chrono::duration<double>(dt));
}
}  // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string> rest;
        auto ta = cli::parse_transport_args(cli::args_from(argc, argv), rest);

        std::string csv_path;
        double rate = 10.0;
        bool realtime = false, loop = false;
        std::optional<std::uint64_t> count;
        for (std::size_t i = 0; i < rest.size(); ++i) {
            const std::string& a = rest[i];
            if (a == "--rate" && i + 1 < rest.size()) {
                rate = std::stod(rest[++i]);
            } else if (a == "--realtime") {
                realtime = true;
            } else if (a == "--loop") {
                loop = true;
            } else if (a == "--count" && i + 1 < rest.size()) {
                count = std::stoull(rest[++i]);
            } else if (!a.empty() && a[0] != '-' && csv_path.empty()) {
                csv_path = a;
            } else {
                std::fprintf(stderr, "attitude_publisher: unexpected argument '%s'\n",
                             a.c_str());
                return 2;
            }
        }
        if (csv_path.empty()) {
            std::fprintf(stderr, "attitude_publisher: a CSV file argument is required\n");
            return 2;
        }

        auto rows = read_attitude_csv(csv_path);
        if (realtime) {
            for (const auto& r : rows) {
                if (!r.t_ms) throw std::runtime_error("--realtime needs a 't_ms' column on every row");
            }
        }

        std::signal(SIGINT, on_sigint);
        FramedLink link(cli::open_transport(ta));
        std::string mode = realtime ? "realtime" : (std::to_string(rate) + " Hz");
        std::printf("[publisher] replaying %zu sample(s) from %s (%s%s) — Ctrl-C to stop\n",
                    rows.size(), csv_path.c_str(), mode.c_str(), loop ? ", looping" : "");
        std::fflush(stdout);

        const double period = 1.0 / rate;
        auto start = std::chrono::steady_clock::now();
        std::uint64_t emitted = 0;
        bool done = false;
        while (!done && !g_stop) {
            std::optional<std::uint32_t> prev_t_ms;
            for (const auto& row : rows) {
                if (g_stop || (count && emitted >= *count)) {
                    done = true;
                    break;
                }
                std::uint32_t seq =
                    (loop || !row.seq) ? static_cast<std::uint32_t>(emitted) : *row.seq;
                std::uint32_t t_ms = row.t_ms ? *row.t_ms
                                              : static_cast<std::uint32_t>(std::lround(
                                                    static_cast<double>(emitted) / rate * 1000.0));
                codec::AttitudeSample msg;
                msg.seq = seq;
                msg.t_ms = t_ms;
                msg.roll = row.roll;
                msg.pitch = row.pitch;
                msg.yaw = row.yaw;
                link.send(msg);
                ++emitted;

                if (realtime) {
                    if (prev_t_ms) sleep_seconds((static_cast<double>(*row.t_ms) -
                                                  static_cast<double>(*prev_t_ms)) / 1000.0);
                    prev_t_ms = row.t_ms;
                } else {
                    auto target = start + std::chrono::duration_cast<
                        std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(static_cast<double>(emitted) * period));
                    sleep_seconds(
                        std::chrono::duration<double>(target - std::chrono::steady_clock::now())
                            .count());
                }
            }
            if (!loop) done = true;
        }

        link.close();
        std::printf("\n[publisher] stopped after %llu sample(s)\n",
                    static_cast<unsigned long long>(emitted));
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "attitude_publisher: %s\n", e.what());
        return 1;
    }
}
