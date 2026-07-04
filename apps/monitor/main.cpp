// monitor — receive the attitude stream and show it live.
//
// Decodes AttitudeSample frames off the link and refreshes a single terminal
// line in place with the current attitude, sample count, and the measured rate.
// Bad-FCS / unknown frames are counted and skipped. Optionally records every
// sample to CSV with --log FILE.
//
//   rs422-monitor --tcp 127.0.0.1:5555 --listen
//   rs422-monitor --pty
//   rs422-monitor --port /dev/ttyUSB0 --log attitude.csv
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>

#include "serial_link/attitude_log.hpp"
#include "serial_link/cli.hpp"
#include "serial_link/link.hpp"

using namespace serial_link;

namespace {
std::atomic<bool> g_stop{false};
void on_sigint(int) { g_stop = true; }

std::string format_line(const codec::AttitudeSample& s, double rate_hz, std::uint64_t drops) {
    char buf[256];
    std::string drop_str = drops ? "  drops=" + std::to_string(drops) : "";
    std::snprintf(buf, sizeof(buf),
                  "roll=%+7.1f\xC2\xB0  pitch=%+7.1f\xC2\xB0  yaw=%6.1f\xC2\xB0   "
                  "seq=%-6u rate=%4.1fHz%s",
                  static_cast<double>(s.roll), static_cast<double>(s.pitch),
                  static_cast<double>(s.yaw), s.seq, rate_hz, drop_str.c_str());
    return buf;
}
}  // namespace

int main(int argc, char** argv) {
    std::string log_path;
    try {
        std::vector<std::string> rest;
        auto ta = cli::parse_transport_args(cli::args_from(argc, argv), rest);
        for (std::size_t i = 0; i < rest.size(); ++i) {
            if (rest[i] == "--log" && i + 1 < rest.size()) {
                log_path = rest[++i];
            } else {
                std::fprintf(stderr, "monitor: unexpected argument '%s'\n", rest[i].c_str());
                return 2;
            }
        }

        std::signal(SIGINT, on_sigint);
        FramedLink link(cli::open_transport(ta));

        std::unique_ptr<AttitudeCsvLogger> logger;
        if (!log_path.empty()) {
            logger = std::make_unique<AttitudeCsvLogger>(log_path);
            std::printf("[monitor] logging samples to %s\n", log_path.c_str());
        }
        std::printf("[monitor] waiting for attitude (Ctrl-C to stop)\n");
        std::fflush(stdout);

        std::uint64_t count = 0, drops = 0, errors = 0;
        std::optional<std::uint32_t> expected_seq;
        double rate_hz = 0.0;
        std::optional<std::chrono::steady_clock::time_point> last_t;

        while (!g_stop) {
            auto rf = link.next(0.2);
            if (!rf) {
                if (link.at_eof()) break;
                continue;  // timeout — re-check the stop flag
            }
            if (!rf->ok() || !std::holds_alternative<codec::AttitudeSample>(*rf->message)) {
                ++errors;
                continue;
            }
            const auto& sample = std::get<codec::AttitudeSample>(*rf->message);
            ++count;
            if (logger) logger->write(sample);

            // gap detection from the seq counter
            if (expected_seq && sample.seq != *expected_seq) {
                drops += static_cast<std::uint32_t>(sample.seq - *expected_seq);
            }
            expected_seq = sample.seq + 1;

            // measured rate: EMA over inter-arrival time
            auto now = std::chrono::steady_clock::now();
            if (last_t) {
                double dt = std::chrono::duration<double>(now - *last_t).count();
                if (dt > 0) {
                    double inst = 1.0 / dt;
                    rate_hz = (rate_hz == 0.0) ? inst : 0.8 * rate_hz + 0.2 * inst;
                }
            }
            last_t = now;

            std::printf("\r%s", format_line(sample, rate_hz, drops).c_str());
            std::fflush(stdout);
        }

        link.close();
        std::string log_str =
            logger ? ", logged " + std::to_string(logger->count()) + " to " + log_path : "";
        std::printf("\n[monitor] stopped — %" PRIu64 " sample(s), %" PRIu64
                    " dropped, %" PRIu64 " bad frame(s)%s\n",
                    count, drops, errors, log_str.c_str());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "monitor: %s\n", e.what());
        return 1;
    }
}
