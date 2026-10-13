// The local server as the game starts it (docs/networking.md, "The local
// server"): `autocraft-server` as a hidden child process, the one-time token
// as the first line of its standard input, its port read from the `PORT <n>`
// line it prints. Stopped with SIGTERM, then waited for (SIGKILL after the
// wait). POSIX (the Mac and Linux); elsewhere `start` says it cannot. Like
// the rest of the library it reads no clock: the caller times the start.
#pragma once

#include "AutocraftNetApi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ac::net {

class AUTOCRAFTNET_API LocalServer {
public:
    enum class State { idle, starting, running, exited };

    LocalServer() = default;
    /// Stops it (`stop(3)`).
    ~LocalServer();
    LocalServer(const LocalServer&) = delete;
    LocalServer& operator=(const LocalServer&) = delete;

    /// Starts `program` with `args` (not counting the program) and writes
    /// `token` and a newline to its standard input. Its standard error goes
    /// nowhere (it has its own log). False, with `error()`, when it does not
    /// start.
    bool start(const std::string& program, const std::vector<std::string>& args, const std::string& token);
    /// Reads what it printed (never waits) and whether it is still alive.
    /// `starting` until the `PORT <n>` line, then `running`; `exited` once
    /// it is gone (`error()` says how).
    State poll();
    State state() const { return state_; }
    uint16_t port() const { return port_; }
    int pid() const { return pid_; }
    /// Why it did not start or how it ended: the reason it printed
    /// (`FAILED <why>` on its standard output: "save … locked (another server
    /// has it open)"), else "exited with code 1".
    const std::string& error() const { return error_; }
    /// The exit code, or 128 + the signal; -1 while it runs.
    int exitCode() const { return exitCode_; }

    /// SIGTERM, then up to `waitSeconds` for it to end, then SIGKILL. True
    /// when it ended on SIGTERM.
    bool stop(double waitSeconds = 3.0);

private:
    void exited(int status);
    /// Reads what it printed so far: the `PORT` line, a `FAILED` reason.
    void readOut();
    void closePipes();

    State state_ = State::idle;
    int pid_ = -1;
    int out_ = -1;
    uint16_t port_ = 0;
    int exitCode_ = -1;
    std::string line_;
    std::string error_;
};

} // namespace ac::net
