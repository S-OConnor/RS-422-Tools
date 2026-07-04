// c2 — command & control host (the register "master").
//
// Sends a single ReadRegister / WriteRegister request and prints the decoded
// reply. Correlation is implicit request/response (link.request): send -> await
// -> print.
//
//   rs422-c2 --tcp 127.0.0.1:5555 read 0x10 4
//   rs422-c2 --port /dev/ttyUSB0 write 0x20 0x1234
//   rs422-c2 --tcp 127.0.0.1:5555 --log c2.csv read 0x10
#include <chrono>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include "serial_link/cli.hpp"
#include "serial_link/link.hpp"

using namespace serial_link;

namespace {

// Host-side transaction audit log (the c2_log.py counterpart, host role only).
// Columns: ts_unix, role, op, addr, arg, result, detail.
void append_log(const std::string& path, const std::string& op, std::uint16_t addr,
                long arg, const std::string& result, const std::string& detail) {
    bool exists = false;
    if (std::FILE* probe = std::fopen(path.c_str(), "r")) {
        std::fseek(probe, 0, SEEK_END);
        exists = std::ftell(probe) > 0;
        std::fclose(probe);
    }
    std::FILE* f = std::fopen(path.c_str(), "a");
    if (!f) throw std::runtime_error("cannot open log file " + path);
    if (!exists) std::fputs("ts_unix,role,op,addr,arg,result,detail\n", f);
    double ts = std::chrono::duration<double>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
    std::fprintf(f, "%.6f,host,%s,0x%04X,%ld,%s,%s\n", ts, op.c_str(), addr, arg,
                 result.c_str(), detail.c_str());
    std::fclose(f);
}

std::string format_reply(const codec::Message& msg) {
    char buf[512];
    if (const auto* r = std::get_if<codec::ReadResponse>(&msg)) {
        std::string vals;
        for (std::size_t i = 0; i < r->values.size(); ++i) {
            char v[32];
            std::snprintf(v, sizeof(v), "[%#06x]=0x%04X",
                          static_cast<unsigned>(r->addr + i), r->values[i]);
            if (i) vals += "  ";
            vals += v;
        }
        std::snprintf(buf, sizeof(buf), "OK  read %zu reg(s): %s", r->values.size(),
                      vals.c_str());
        return buf;
    }
    if (const auto* w = std::get_if<codec::WriteAck>(&msg)) {
        std::snprintf(buf, sizeof(buf), "OK  wrote [%#06x]=0x%04X (status %u)", w->addr,
                      w->value, w->status);
        return buf;
    }
    if (const auto* n = std::get_if<codec::Nak>(&msg)) {
        std::snprintf(buf, sizeof(buf), "NAK addr=%#06x: %s", n->addr, n->reason().c_str());
        return buf;
    }
    return "unexpected reply: " + codec::to_string(msg);
}

// Parse an integer with base autodetect (0x.. hex, plain decimal), like Python int(s, 0).
long parse_int(const std::string& s) { return std::stol(s, nullptr, 0); }

}  // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string> rest;
        auto ta = cli::parse_transport_args(cli::args_from(argc, argv), rest);

        double timeout = 1.0;
        std::string log_path, op;
        std::vector<std::string> pos;
        for (std::size_t i = 0; i < rest.size(); ++i) {
            const std::string& a = rest[i];
            if (a == "--timeout" && i + 1 < rest.size()) {
                timeout = std::stod(rest[++i]);
            } else if (a == "--log" && i + 1 < rest.size()) {
                log_path = rest[++i];
            } else {
                pos.push_back(a);
            }
        }
        if (pos.empty() || (pos[0] != "read" && pos[0] != "write")) {
            std::fprintf(stderr, "c2: expected 'read ADDR [COUNT]' or 'write ADDR VALUE'\n");
            return 2;
        }
        op = pos[0];

        codec::Message request;
        std::uint16_t addr = 0;
        long arg = 0;
        if (op == "read") {
            if (pos.size() < 2) {
                std::fprintf(stderr, "c2: read needs an address\n");
                return 2;
            }
            addr = static_cast<std::uint16_t>(parse_int(pos[1]));
            long cnt = pos.size() >= 3 ? parse_int(pos[2]) : 1;
            arg = cnt;
            request = codec::ReadRegister{addr, static_cast<std::uint8_t>(cnt)};
        } else {
            if (pos.size() < 3) {
                std::fprintf(stderr, "c2: write needs an address and a value\n");
                return 2;
            }
            addr = static_cast<std::uint16_t>(parse_int(pos[1]));
            long value = parse_int(pos[2]);
            arg = value;
            request = codec::WriteRegister{addr, static_cast<std::uint16_t>(value)};
        }

        FramedLink link(cli::open_transport(ta));
        std::optional<ReceivedFrame> reply;
        if (op == "read") {
            reply = link.request(std::get<codec::ReadRegister>(request), timeout);
        } else {
            reply = link.request(std::get<codec::WriteRegister>(request), timeout);
        }
        link.close();

        // Decide outcome, then log it (host perspective) before printing.
        std::string result, detail;
        if (!reply) {
            result = "timeout";
        } else if (!reply->ok()) {
            result = "error";
        } else {
            const auto& m = *reply->message;
            if (const auto* n = std::get_if<codec::Nak>(&m)) {
                result = "nak";
                detail = n->reason();
            } else if (const auto* r = std::get_if<codec::ReadResponse>(&m)) {
                result = "ok";
                for (std::size_t i = 0; i < r->values.size(); ++i) {
                    char v[16];
                    std::snprintf(v, sizeof(v), "0x%04X", r->values[i]);
                    if (i) detail += ' ';
                    detail += v;
                }
            } else if (const auto* w = std::get_if<codec::WriteAck>(&m)) {
                result = "ok";
                detail = "status=" + std::to_string(w->status);
            } else {
                result = "other";
            }
        }
        if (!log_path.empty()) append_log(log_path, op, addr, arg, result, detail);

        if (!reply) {
            std::printf("timeout after %gs — no reply\n", timeout);
            return 1;
        }
        if (!reply->ok()) {
            std::printf("bad reply frame: %s\n", reply->error.c_str());
            return 1;
        }
        std::printf("%s\n", format_reply(*reply->message).c_str());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "c2: %s\n", e.what());
        return 1;
    }
}
