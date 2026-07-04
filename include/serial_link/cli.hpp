// Shared CLI plumbing: turn --port/--tcp/--pty flags into a Transport.
//
// Reused by every serial-side app so the transport-selection UX stays identical
// everywhere, matching the Python serial_link.cli.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "serial_link/transport.hpp"

namespace serial_link::cli {

// The parsed transport selection. Exactly one of {port, tcp, pty} is chosen.
struct TransportArgs {
    std::string port;    // --port DEV   (real serial cable)
    std::string tcp;     // --tcp HOST:PORT
    bool pty = false;    // --pty
    int baud = 115200;   // --baud
    bool listen = false; // --listen (with --tcp: accept instead of dial)
};

// Consume the shared transport flags from `args`, leaving app-specific tokens in
// `rest`. Throws std::runtime_error on a malformed/missing transport selection.
TransportArgs parse_transport_args(const std::vector<std::string>& args,
                                   std::vector<std::string>& rest);

// Build a Transport from parsed args. Prints the pty slave path / listen banner,
// matching the Python CLI.
std::unique_ptr<Transport> open_transport(const TransportArgs& args);

// argv[1..] as a vector<string> (small helper for app main()s).
std::vector<std::string> args_from(int argc, char** argv);

}  // namespace serial_link::cli
