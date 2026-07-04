#include "serial_link/cli.hpp"

#include <cstdio>
#include <stdexcept>

namespace serial_link::cli {

std::vector<std::string> args_from(int argc, char** argv) {
    std::vector<std::string> out;
    for (int i = 1; i < argc; ++i) out.emplace_back(argv[i]);
    return out;
}

TransportArgs parse_transport_args(const std::vector<std::string>& args,
                                   std::vector<std::string>& rest) {
    TransportArgs ta;
    int selected = 0;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        auto need_value = [&](const char* flag) -> std::string {
            if (i + 1 >= args.size()) {
                throw std::runtime_error(std::string(flag) + " requires a value");
            }
            return args[++i];
        };
        if (a == "--port") {
            ta.port = need_value("--port");
            ++selected;
        } else if (a == "--tcp") {
            ta.tcp = need_value("--tcp");
            ++selected;
        } else if (a == "--pty") {
            ta.pty = true;
            ++selected;
        } else if (a == "--baud") {
            ta.baud = std::stoi(need_value("--baud"));
        } else if (a == "--listen") {
            ta.listen = true;
        } else {
            rest.push_back(a);
        }
    }
    if (selected != 1) {
        throw std::runtime_error(
            "select exactly one transport: --port DEV | --tcp HOST:PORT | --pty");
    }
    return ta;
}

std::unique_ptr<Transport> open_transport(const TransportArgs& args) {
    if (!args.port.empty()) {
        return std::make_unique<SerialTransport>(args.port, args.baud);
    }
    if (!args.tcp.empty()) {
        auto colon = args.tcp.find(':');
        if (colon == std::string::npos) {
            throw std::runtime_error("--tcp expects HOST:PORT");
        }
        std::string host = args.tcp.substr(0, colon);
        int port = std::stoi(args.tcp.substr(colon + 1));
        if (args.listen) {
            std::printf("[listening on %s:%d ...]\n", host.c_str(), port);
            std::fflush(stdout);
            return TcpTransport::listen_one(host, port);
        }
        return TcpTransport::connect(host, port);
    }
    auto pty = std::make_unique<PtyTransport>();
    std::printf("[pty ready] point the peer at: %s\n", pty->slave_name.c_str());
    std::fflush(stdout);
    return pty;
}

}  // namespace serial_link::cli
